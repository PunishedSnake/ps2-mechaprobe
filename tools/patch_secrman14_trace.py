#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned PS2SDK SECRMAN 1.4 for dev.9 passive auth/session tracing.

Important: this patch does NOT start a second SecrAuthCard transaction from the KELF
path. MCMAN already authenticates a PS2 memory card during its normal card probe.
dev.9 passively records that stock MCMAN -> SecrAuthCard handshake, then preserves
it until the subsequent Candidate-A KELF transaction reaches ICVPS2.

Captured without replaying commands:
- CardIV, CardMaterial and CardNonce
- MechaChallenge1/2/3
- CardResponse1/2/3
- raw pre-CardAuth 0x94/0x95 Kbit and 0x96/0x97 Kc
- normal final Kbit/Kc and ICVPS2

The 72-byte auth transcript plus an 8-byte header is copied to unused offset 0x100
of the final ICVPS2 RPC reply. Kbit/Kc pre-values continue to use offset 0x100 of
their independent 0x1000-byte RPC replies.
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
SECRMAN = ROOT / "src" / "secrman.c"
TRACE_RELATIVE_OFFSET = 0xF8
AUTH_RELATIVE_OFFSET = 0x100


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

marker = "extern struct irx_export_table _exp_secrman;\n"
if marker not in text:
    raise SystemExit("missing SECRMAN export marker")
text = text.replace(marker, marker + r'''

/* dev.9 passive capture of the normal MCMAN-initiated card authentication. */
static unsigned char MgTraceAuth[80];
static int MgTraceAuthValid;

static void MgTraceResetAuth(void)
{
    memset(MgTraceAuth, 0, sizeof(MgTraceAuth));
    MgTraceAuth[0] = 0x4d; /* M */
    MgTraceAuth[1] = 0x47; /* G */
    MgTraceAuth[2] = 0x41; /* A */
    MgTraceAuth[3] = 0x39; /* 9 */
    MgTraceAuthValid = 0;
}
''', 1)

# Instrument the stock SecrAuthCard used by MCMAN. Reset at the start of the
# real authentication and capture only after the complete handshake succeeds.
start, end = find_function(text, "int SecrAuthCard(")
auth = text[start:end]
first_check = "\n\n    if (GetMcCommandHandler() == NULL) {"
if first_check not in auth:
    raise SystemExit("SecrAuthCard first-check marker not found")
auth = auth.replace(first_check,
                    "\n\n    MgTraceResetAuth();\n" + first_check[1:], 1)

success_marker = "    return 1;\n\nError2_end:"
if success_marker not in auth:
    raise SystemExit("SecrAuthCard success marker not found")
capture = r'''    /* This is the exact successful authentication performed by MCMAN. */
    MgTraceAuth[4] = 1;
    MgTraceAuth[5] = (unsigned char)cnum;
    MgTraceAuth[6] = (unsigned char)port;
    MgTraceAuth[7] = (unsigned char)slot;
    memcpy(&MgTraceAuth[8],  CardIV, 8);
    memcpy(&MgTraceAuth[16], CardMaterial, 8);
    memcpy(&MgTraceAuth[24], CardNonce, 8);
    memcpy(&MgTraceAuth[32], MechaChallenge1, 8);
    memcpy(&MgTraceAuth[40], MechaChallenge2, 8);
    memcpy(&MgTraceAuth[48], MechaChallenge3, 8);
    memcpy(&MgTraceAuth[56], CardResponse1, 8);
    memcpy(&MgTraceAuth[64], CardResponse2, 8);
    memcpy(&MgTraceAuth[72], CardResponse3, 8);
    MgTraceAuthValid = 1;

    return 1;

Error2_end:'''
auth = auth.replace(success_marker, capture, 1)
text = text[:start] + auth + text[end:]

new_kbit = f'''int SecrDownloadGetKbit(int port, int slot, void *kbit)
{{
    if (scePreEncryptKbit(kbit) == 0) {{
        return 0;
    }}

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

    memcpy((void *)((unsigned char *)kc + 0x{TRACE_RELATIVE_OFFSET:02x}), kc, 16);

    if (card_encrypt(port, slot, kc) == 0) {{
        return 0;
    }}
    if (card_encrypt(port, slot, (void *)((unsigned char *)kc + 8)) == 0) {{
        return 0;
    }}
    return 1;
}}'''

new_icv = f'''int SecrDownloadGetICVPS2(void *icvps2)
{{
    if (func_00000d14(icvps2) == 0) {{
        return 0;
    }}

    if (MgTraceAuthValid)
        memcpy((void *)((unsigned char *)icvps2 + 0x{AUTH_RELATIVE_OFFSET:03x}), MgTraceAuth, sizeof(MgTraceAuth));

    return 1;
}}'''

text = replace_function(text, "int SecrDownloadGetKbit(", new_kbit)
text = replace_function(text, "int SecrDownloadGetKc(", new_kc)
text = replace_function(text, "int SecrDownloadGetICVPS2(", new_icv)
SECRMAN.write_text(text)

print("PS2SDK SECRMAN 1.4 dev.9 passive MCMAN-auth/session instrumentation applied")
