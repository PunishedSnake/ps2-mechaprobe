# MechaCon / MagicGate keystore input verification (2026-11-09)

## Status and provenance

**POTWIERDZONE (byte identity):** Four operator-supplied binary files were inspected **locally**, with exact lengths, SHA-1 and SHA-256 calculated. All four SHA-1 digests match the canonical file list used by the older PCSX2 Python 2 MagicGate setup. PS2 Developer Wiki documents the corresponding CIV, CKS, EKS and KEK material.

**Not a claim of a novel worldwide key extraction.** These bytes/hashes are documented publicly. Their availability is a major milestone for reproducing and testing the existing ps2-mechaprobe MagicGate model offline. The means by which the operator obtained the files is **not established by these hashes**.

**Do not commit the four raw `.bin` files, plaintext derived keystores or auth secrets to this repository.** Only metadata is recorded. Keep the operator's copies outside the tracked tree.

Sources:
- https://www.psdevwiki.com/ps2/Talk:Keys
- https://github.com/DiscoStarslayer/pcsx2-reliquary/blob/master/README.md
- https://github-wiki-see.page/m/987123879113/pcsx2/wiki/Setup-%28wxWidgets-build%29

## Exact checksums

| Filename | Size (B) | SHA-1 (matching public reference) | SHA-256 (local evidence) |
|---|---:|---|---|
| `civ.bin` | 8 | `5354987f01d9d5e1390ef9946342d1ecad6211b1` | `e7b02f4f8d99a58b96dbca4db81c5d666ea7c46fbf6e1d5c045eaba0ee25416a` |
| `cks.bin` | 96 | `a793f867b0325e67982ba8e53f659480c1f512bb` | `fef2015096181409b25fb4c4cd0e0fc48ca73c6ea845c0ed785c06bf9becd84e` |
| `eks.bin` | 1024 | `5a3a3ac6f03cf100ef6e8bc92d22e91ec14e27cf` | `04bcc6b13827829fb5cc8dbd86420d30f69a2bfd3b7719398b341e15368bd365` |
| `kek.bin` | 16 | `77227dbaaea1042f5972e65a56d2234024e8eff8` | `f7c9233b37a7662a882eca096f43b35af74b3de6030c1ad5185692332e96aecb` |

## Names and scope

- `civ.bin`: MechaCon MagicGate challenge-IV material; its eight bytes match the public `MG_CHALLENGE_IV`.
- `cks.bin`: card key store data (96 B), not a dynamic per-transaction session key.
- `eks.bin`: encrypted MechaCon key store data (1024 B).
- `kek.bin`: key for the encrypted key store (16 B). **This is not automatically the same as the KELF-header-derived KEK** calculated by our existing Candidate-A/C/D generator.

The documented PCSX2 Reliquary security settings accept all four for authenticated retail/arcade/Python 2 paths. That establishes file semantics; it does **not** by itself prove an end-to-end hardware-equivalent implementation in our probe.

## Local reproducibility

The repository's `tools/verify_mechacon_keystore.py` verifies all four files with length, SHA-1 and SHA-256, without printing key bytes or writing derived material:

```sh
python3 tools/verify_mechacon_keystore.py --directory /path/to/private/keys
```

Use a directory **outside** the repository and do not attach raw key files to public GitHub issues or logs.

## Relation to the existing hardware evidence

Earlier known-good retail-hardware datasets remain unchanged:

- Candidate A: valid Uses_ICVPS2 KELF; transaction-variable 0x98.
- Candidate C: four distinct plaintext Kbit/Kc halves; four different MechaCon pre-card outputs per session; stable card-dependent final F2 outputs across cold boots.
- Candidate D: repeated A/B/A/B; on the third-party 64 MB card, equal pre-F2 input can give different outputs due to hidden card-side context.
- dev.11: valid Candidate-C KELF transaction with no **successful** full `SecrAuthCard()` observed in the instrumented window. A later count-only probe was built to distinguish zero calls from failed calls.

**Current inference:** static keystore material should allow independently implementing/verifying parts of MagicGate key selection and challenge handling. It does **not** directly reveal the live `SessionKey` from any particular prior cold-boot RUN; those RUNs need the corresponding handshake/transcript or another independently justified key derivation. Nor does it by itself establish the formula for ICVPS2.

### Next controlled validation

1. Establish keystore semantics using an explicit pinned implementation of the EKS/KEK/CKS decoder. Use offline known-answer vectors and check parity with publicly documented keystore entries; no speculative DES mode.
2. Keep the decrypted keystore and generated traces **out of the public repository**. A correctness hash and script/test vector metadata may be committed.
3. Derive a session key only when the input transcript/data are available and anchored to the same transaction. Validate against **all four** Candidate-C plaintext-to-pre-key pairs, not a single 8-byte output.
4. Then attempt a separate hypothesis-driven test for SCMD `0x98` / ICVPS2. Never label ICV generation solved from static key files alone.

All claims of protocol behavior remain subject to real-hardware confirmation; emulator results are explanatory and useful for correctness, not authoritative hardware reproduction.
