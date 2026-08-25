#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Generate controlled ICVPS2 KELF test vectors from the hardware-validated
Candidate-A layout.

This tool deliberately does not contain PlayStation 2 cryptographic keys.
Provide a kelftool-compatible PS2KEYS.dat containing the required keys.

Supported template contract (intentionally narrow):
- PS2 KELF
- flags 0x022e (Uses_ICVPS2)
- header size 0x88
- Candidate-A-style layout:
  base header 0x20
  header signature 0x08
  encrypted Kbit 0x10
  encrypted Kc 0x10
  encrypted 0x28-byte BIT table
  BIT signature 0x08
  root signature 0x08
  ICVPS2 slot 0x08
- two BIT blocks:
  block 0: 0x20 bytes, flags 0x3
  block 1: remainder, flags 0x0

The narrow contract is intentional: it makes the research vector generator
auditable and prevents this experimental script from pretending to be a
general-purpose KELF implementation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, modes
    from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
except Exception as exc:  # pragma: no cover
    raise SystemExit(
        "This research tool requires Python package 'cryptography'. "
        f"Import failed: {exc}"
    )

ZERO8 = b"\x00" * 8

REQUIRED_KEYS = {
    "MG_SIG_MASTER_KEY": 8,
    "MG_SIG_HASH_KEY": 8,
    "MG_KBIT_MASTER_KEY": 16,
    "MG_KBIT_IV": 8,
    "MG_KC_MASTER_KEY": 16,
    "MG_KC_IV": 8,
    "MG_ROOTSIG_MASTER_KEY": 8,
    "MG_ROOTSIG_HASH_KEY": 16,
    "MG_CONTENT_TABLE_IV": 8,
    "MG_CONTENT_IV": 8,
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def xor_bytes(a: bytes, b: bytes) -> bytes:
    if len(a) != len(b):
        raise ValueError("xor length mismatch")
    return bytes(x ^ y for x, y in zip(a, b))


def cbc_encrypt(data: bytes, key: bytes, iv: bytes) -> bytes:
    if len(data) % 8:
        raise ValueError("DES-CBC input must be a multiple of 8 bytes")
    enc = Cipher(TripleDES(key), modes.CBC(iv)).encryptor()
    return enc.update(data) + enc.finalize()


def cbc_decrypt(data: bytes, key: bytes, iv: bytes) -> bytes:
    if len(data) % 8:
        raise ValueError("DES-CBC input must be a multiple of 8 bytes")
    dec = Cipher(TripleDES(key), modes.CBC(iv)).decryptor()
    return dec.update(data) + dec.finalize()


def load_keys(path: Path) -> dict[str, bytes]:
    parsed: dict[str, bytes] = {}
    for raw in path.read_text(encoding="ascii").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if "=" not in line:
            continue
        name, value = line.split("=", 1)
        name = name.strip()
        if name not in REQUIRED_KEYS:
            continue
        value = value.strip().replace(" ", "")
        try:
            parsed[name] = bytes.fromhex(value)
        except ValueError as exc:
            raise SystemExit(f"Invalid hex for {name}: {exc}") from exc

    missing = [name for name in REQUIRED_KEYS if name not in parsed]
    if missing:
        raise SystemExit("Missing keys: " + ", ".join(missing))

    for name, expected in REQUIRED_KEYS.items():
        actual = len(parsed[name])
        if actual != expected:
            raise SystemExit(
                f"{name}: expected {expected} bytes, got {actual}"
            )
    return parsed


def derive_kek(base_header: bytes, keys: dict[str, bytes]) -> bytes:
    header_data = xor_bytes(base_header[:8], base_header[8:16])
    return (
        cbc_encrypt(
            header_data,
            keys["MG_KBIT_MASTER_KEY"],
            keys["MG_KBIT_IV"],
        )
        + cbc_encrypt(
            header_data,
            keys["MG_KC_MASTER_KEY"],
            keys["MG_KC_IV"],
        )
    )


def get_header_signature(base_header: bytes, keys: dict[str, bytes]) -> bytes:
    tmp = cbc_encrypt(base_header, keys["MG_SIG_MASTER_KEY"], ZERO8)[-8:]
    tmp = cbc_decrypt(tmp, keys["MG_SIG_HASH_KEY"], ZERO8)
    return cbc_encrypt(tmp, keys["MG_SIG_MASTER_KEY"], ZERO8)


def get_block_signature(plain: bytes, keys: dict[str, bytes]) -> bytes:
    if len(plain) % 8:
        raise ValueError("signed block must be an 8-byte multiple")
    acc = ZERO8
    for off in range(0, len(plain), 8):
        acc = xor_bytes(acc, plain[off:off + 8])
    return cbc_encrypt(
        acc,
        keys["MG_SIG_MASTER_KEY"] + keys["MG_SIG_HASH_KEY"],
        ZERO8,
    )


def get_bit_signature(
    kbit: bytes,
    kc: bytes,
    bit_table_plain: bytes,
    keys: dict[str, bytes],
) -> bytes:
    acc = kbit[:8]
    if kbit[:8] != kbit[8:16]:
        acc = xor_bytes(acc, kbit[8:16])
    acc = xor_bytes(acc, kc[:8])
    if kc[:8] != kc[8:16]:
        acc = xor_bytes(acc, kc[8:16])

    for off in range(0, len(bit_table_plain), 8):
        acc = xor_bytes(acc, bit_table_plain[off:off + 8])

    return cbc_encrypt(
        acc,
        keys["MG_SIG_MASTER_KEY"] + keys["MG_SIG_HASH_KEY"],
        ZERO8,
    )


def get_root_signature(
    header_signature: bytes,
    bit_signature: bytes,
    signed_block_signatures: list[bytes],
    keys: dict[str, bytes],
) -> bytes:
    material = header_signature + bit_signature + b"".join(
        signed_block_signatures
    )
    encrypted = cbc_encrypt(
        material,
        keys["MG_ROOTSIG_MASTER_KEY"],
        ZERO8,
    )
    return cbc_decrypt(
        encrypted[-8:],
        keys["MG_ROOTSIG_HASH_KEY"],
        ZERO8,
    )


def encrypt_content_key_half(plain: bytes, kek: bytes) -> bytes:
    return cbc_encrypt(plain, kek, ZERO8)


def decrypt_content_key_half(ciphertext: bytes, kek: bytes) -> bytes:
    return cbc_decrypt(ciphertext, kek, ZERO8)


def validate_template(data: bytes, keys: dict[str, bytes]) -> dict:
    if len(data) < 0xA8:
        raise SystemExit("Template is too small")

    base = data[:0x20]
    header_size = struct.unpack_from("<H", base, 0x14)[0]
    flags = struct.unpack_from("<H", base, 0x18)[0]
    bit_count = struct.unpack_from("<H", base, 0x1A)[0]

    if header_size != 0x88:
        raise SystemExit(
            f"Unsupported template header size 0x{header_size:04x}; expected 0x0088"
        )
    if flags != 0x022E:
        raise SystemExit(
            f"Unsupported template flags 0x{flags:04x}; expected 0x022e"
        )
    if bit_count != 0:
        raise SystemExit(
            f"Unsupported base-header BIT_count={bit_count}; expected 0"
        )

    stored_header_sig = data[0x20:0x28]
    calc_header_sig = get_header_signature(base, keys)
    if stored_header_sig != calc_header_sig:
        raise SystemExit("Template header signature verification failed")

    kek = derive_kek(base, keys)

    old_kbit = (
        decrypt_content_key_half(data[0x28:0x30], kek)
        + decrypt_content_key_half(data[0x30:0x38], kek)
    )
    old_kc = (
        decrypt_content_key_half(data[0x38:0x40], kek)
        + decrypt_content_key_half(data[0x40:0x48], kek)
    )

    bit_table_plain = cbc_decrypt(
        data[0x48:0x70],
        old_kbit,
        keys["MG_CONTENT_TABLE_IV"],
    )
    bt_header_size, bt_block_count = struct.unpack_from(
        "<II", bit_table_plain, 0
    )
    if bt_header_size != 0x88 or bt_block_count != 2:
        raise SystemExit(
            "Unsupported BIT layout: expected header_size=0x88 and block_count=2"
        )

    blocks = []
    for index in range(bt_block_count):
        off = 8 + index * 16
        size, block_flags = struct.unpack_from("<II", bit_table_plain, off)
        signature = bit_table_plain[off + 8:off + 16]
        blocks.append(
            {
                "size": size,
                "flags": block_flags,
                "signature": signature,
            }
        )

    if blocks[0]["size"] != 0x20 or blocks[0]["flags"] != 0x3:
        raise SystemExit(
            "Unsupported block 0; expected size=0x20 flags=0x3"
        )
    if blocks[1]["flags"] != 0x0:
        raise SystemExit("Unsupported block 1; expected flags=0")

    payload = data[0x88:]
    if blocks[0]["size"] + blocks[1]["size"] != len(payload):
        raise SystemExit("BIT sizes do not cover the payload exactly")

    old_block0_plain = cbc_decrypt(
        payload[:0x20],
        old_kc,
        keys["MG_CONTENT_IV"],
    )
    calc_block_sig = get_block_signature(old_block0_plain, keys)
    if calc_block_sig != blocks[0]["signature"]:
        raise SystemExit("Template block signature verification failed")

    calc_bit_sig = get_bit_signature(
        old_kbit,
        old_kc,
        bit_table_plain,
        keys,
    )
    if calc_bit_sig != data[0x70:0x78]:
        raise SystemExit("Template BIT signature verification failed")

    calc_root_sig = get_root_signature(
        stored_header_sig,
        calc_bit_sig,
        [blocks[0]["signature"]],
        keys,
    )
    if calc_root_sig != data[0x78:0x80]:
        raise SystemExit("Template root signature verification failed")

    return {
        "base": base,
        "header_signature": stored_header_sig,
        "kek": kek,
        "old_kbit": old_kbit,
        "old_kc": old_kc,
        "bit_table_plain": bit_table_plain,
        "blocks": blocks,
        "payload": payload,
        "old_block0_plain": old_block0_plain,
    }


def build_candidate(
    template: bytes,
    new_kbit: bytes,
    new_kc: bytes,
    keys: dict[str, bytes],
) -> tuple[bytes, dict]:
    if len(new_kbit) != 16 or len(new_kc) != 16:
        raise SystemExit("Kbit and Kc must each be exactly 16 bytes")

    parsed = validate_template(template, keys)

    base = parsed["base"]
    header_signature = get_header_signature(base, keys)
    kek = parsed["kek"]

    # Recalculate the signed block signature from the plaintext, even though
    # Candidate-C deliberately keeps the payload plaintext byte-identical.
    block0_signature = get_block_signature(
        parsed["old_block0_plain"],
        keys,
    )

    bit_table_plain = bytearray(parsed["bit_table_plain"])
    bit_table_plain[16:24] = block0_signature

    bit_signature = get_bit_signature(
        new_kbit,
        new_kc,
        bytes(bit_table_plain),
        keys,
    )
    root_signature = get_root_signature(
        header_signature,
        bit_signature,
        [block0_signature],
        keys,
    )

    encrypted_kbit = (
        encrypt_content_key_half(new_kbit[:8], kek)
        + encrypt_content_key_half(new_kbit[8:], kek)
    )
    encrypted_kc = (
        encrypt_content_key_half(new_kc[:8], kek)
        + encrypt_content_key_half(new_kc[8:], kek)
    )

    encrypted_bit_table = cbc_encrypt(
        bytes(bit_table_plain),
        new_kbit,
        keys["MG_CONTENT_TABLE_IV"],
    )

    new_payload = bytearray(parsed["payload"])
    new_payload[:0x20] = cbc_encrypt(
        parsed["old_block0_plain"],
        new_kc,
        keys["MG_CONTENT_IV"],
    )

    output = (
        base
        + header_signature
        + encrypted_kbit
        + encrypted_kc
        + encrypted_bit_table
        + bit_signature
        + root_signature
        + ZERO8
        + bytes(new_payload)
    )

    # Full offline re-validation of the produced candidate.
    checked = validate_template(output, keys)
    if checked["old_kbit"] != new_kbit:
        raise SystemExit("Internal verification failed: Kbit mismatch")
    if checked["old_kc"] != new_kc:
        raise SystemExit("Internal verification failed: Kc mismatch")
    if checked["old_block0_plain"] != parsed["old_block0_plain"]:
        raise SystemExit("Internal verification failed: plaintext block changed")

    old_plain_payload = bytearray(parsed["payload"])
    old_plain_payload[:0x20] = parsed["old_block0_plain"]
    new_plain_payload = bytearray(checked["payload"])
    new_plain_payload[:0x20] = checked["old_block0_plain"]
    if old_plain_payload != new_plain_payload:
        raise SystemExit("Internal verification failed: plaintext payload changed")

    manifest = {
        "template_sha256": sha256(template),
        "output_sha256": sha256(output),
        "size": len(output),
        "flags": "0x022e",
        "header_size": "0x0088",
        "plaintext_kbit": new_kbit.hex(),
        "plaintext_kc": new_kc.hex(),
        "plaintext_blocks": [
            new_kbit[:8].hex(),
            new_kbit[8:].hex(),
            new_kc[:8].hex(),
            new_kc[8:].hex(),
        ],
        "header_signature": header_signature.hex(),
        "bit_table_signature": bit_signature.hex(),
        "root_signature": root_signature.hex(),
        "plaintext_payload_sha256": sha256(bytes(new_plain_payload)),
    }
    return output, manifest


def parse_16_byte_hex(value: str, name: str) -> bytes:
    try:
        data = bytes.fromhex(value)
    except ValueError as exc:
        raise SystemExit(f"{name}: invalid hex: {exc}") from exc
    if len(data) != 16:
        raise SystemExit(f"{name}: expected 16 bytes, got {len(data)}")
    return data


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--template", required=True, type=Path)
    parser.add_argument("--keys", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--kbit",
        required=True,
        help="16-byte plaintext Kbit as 32 hex characters",
    )
    parser.add_argument(
        "--kc",
        required=True,
        help="16-byte plaintext Kc as 32 hex characters",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        help="optional JSON manifest output",
    )
    parser.add_argument(
        "--expect-template-sha256",
        help="abort unless template SHA-256 matches",
    )
    args = parser.parse_args()

    template = args.template.read_bytes()
    if (
        args.expect_template_sha256
        and sha256(template).lower()
        != args.expect_template_sha256.lower()
    ):
        raise SystemExit(
            "Template SHA-256 mismatch: "
            f"{sha256(template)} != {args.expect_template_sha256}"
        )

    keys = load_keys(args.keys)
    new_kbit = parse_16_byte_hex(args.kbit, "Kbit")
    new_kc = parse_16_byte_hex(args.kc, "Kc")

    output, manifest = build_candidate(
        template,
        new_kbit,
        new_kc,
        keys,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)

    if args.manifest:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )

    print(f"output: {args.output}")
    print(f"sha256: {manifest['output_sha256']}")
    print(f"plaintext Kbit: {manifest['plaintext_kbit']}")
    print(f"plaintext Kc:   {manifest['plaintext_kc']}")
    print(f"BIT signature:  {manifest['bit_table_signature']}")
    print(f"root signature: {manifest['root_signature']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
