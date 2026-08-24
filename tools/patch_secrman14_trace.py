#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned PS2SDK SECRMAN 1.4 for one-pass KELF crypto tracing.

The patch does not replay or add MechaCon/CardAuth commands. It copies the
16-byte values returned by the normal 0x94/0x95 and 0x96/0x97 sequences into an
unused area of the existing 0x1000-byte SECRSIF RPC reply before card_encrypt()
mutates the ordinary Kbit/Kc output in place.

SECRSIF hands SecrDownloadGetKbit/GetKc pointers at RPC-buffer offset 0x08, so
TRACE_RELATIVE_OFFSET 0xF8 lands at absolute RPC offset 0x100. The standard
response structures occupy only the first few dozen bytes, while libsecr sends
and receives the entire 0x1000-byte buffer.
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
SECRMAN = ROOT / "src" / "secrman.c"
TRACE_RELATIVE_OFFSET = 0xF8


def find_function(text: str, signature_start: str):
    start = text.find(signature_start)
    if start < 0:
        raise SystemExit(f"missing function: {signature_start}")
    brace = text.find("{", start)
    if brace < 0:
        raise SystemExit(f"missing opening brace: {signature_start}")
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
    raise SystemExit(f"unterminated function: {signature_start}")


def replace_function(text: str, signature_start: str, replacement: str):
    start, end = find_function(text, signature_start)
    return text[:start] + replacement + text[end:]


text = SECRMAN.read_text()

new_kbit = f'''int SecrDownloadGetKbit(int port, int slot, void *kbit)
{{
    if (scePreEncryptKbit(kbit) == 0) {{
        return 0;
    }}

    /* Trace the real 0x94/0x95 output before CardAuth mutates it in place. */
    memcpy((void *)((unsigned char *)kbit + 0x{TRACE_RELATIVE_OFFSET:02x}), kbit, 16);

    if (card_encrypt(port, slot, kbit) == 0) {{
        return 0;
    }}
    if (card_encrypt(port, slot, (void *)((unsigned char *)kbit + 8)) == 0) {{
        return 0;
    }}

    return 1;
}}'''

new_kc = f'''int SecrDownloadGetKc(int port, int slot, void *kc)
{{
    if (scePreEncryptKc(kc) == 0) {{
        return 0;
    }}

    /* Trace the real 0x96/0x97 output before CardAuth mutates it in place. */
    memcpy((void *)((unsigned char *)kc + 0x{TRACE_RELATIVE_OFFSET:02x}), kc, 16);

    if (card_encrypt(port, slot, kc) == 0) {{
        return 0;
    }}
    if (card_encrypt(port, slot, (void *)((unsigned char *)kc + 8)) == 0) {{
        return 0;
    }}

    return 1;
}}'''

text = replace_function(text, "int SecrDownloadGetKbit(", new_kbit)
text = replace_function(text, "int SecrDownloadGetKc(", new_kc)
SECRMAN.write_text(text)

print("PS2SDK SECRMAN 1.4 one-pass crypto trace instrumentation applied")
