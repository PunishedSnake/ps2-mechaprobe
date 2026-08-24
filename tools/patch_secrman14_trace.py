#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned PS2SDK SECRMAN 1.4 for dev.8 auth/session tracing.

The normal KELF binding path is retained, but SecrDownloadHeader() first performs
one explicit SecrAuthCard() using the same physical SIO2 port/cnum that will be
used for the KELF transaction. This deliberately establishes a fresh, known
MagicGate session instead of inheriting whatever session the launcher left.

The patch captures, without replaying commands:
- CardIV, CardMaterial and CardNonce
- MechaChallenge1/2/3
- CardResponse1/2/3
- raw pre-CardAuth 0x94/0x95 Kbit and 0x96/0x97 Kc
- the normal final Kbit/Kc and ICVPS2 remain returned through stock RPC fields

The 72-byte auth transcript plus an 8-byte header is copied to unused offset
0x100 of the final ICVPS2 RPC reply. Kbit/Kc pre-values continue to use offset
0x100 of their own independent 0x1000-byte RPC replies.
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

/* dev.8 controlled authentication transcript. */
static unsigned char MgTraceAuth[80];
static int MgTraceAuthValid;

static void MgTraceResetAuth(void)
{
    memset(MgTraceAuth, 0, sizeof(MgTraceAuth));
    MgTraceAuth[0] = 0x4d; /* M */
    MgTraceAuth[1] = 0x47; /* G */
    MgTraceAuth[2] = 0x41; /* A */
    MgTraceAuth[3] = 0x38; /* 8 */
    MgTraceAuthValid = 0;
}
''', 1)

# Replace SecrAuthCard with the stock logic plus an in-place success capture.
start, end = find_function(text, "int SecrAuthCard(")
auth = text[start:end]
needle = "    return 1;\n\nError2_end:"
if needle not in auth:
    raise SystemExit("SecrAuthCard success marker not found")
capture = r'''    /* Capture exactly the successful authentication that established the
       session used by the following KELF binding transaction. */
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
auth = auth.replace(needle, capture, 1)
text = text[:start] + auth + text[end:]

# Establish a fresh explicit card-auth session immediately before normal header
# processing. cnum is still obtained through the stock callback.
start, end = find_function(text, "int SecrDownloadHeader(")
header = text[start:end]
needle = "    if (secr_set_header(2, cnum, 0, buffer) == 0) {"
if needle not in header:
    raise SystemExit("SecrDownloadHeader set-header marker not found")
insert = r'''    MgTraceResetAuth();
    if (SecrAuthCard(port, slot, cnum) == 0) {
        _printf("dev8 explicit SecrAuthCard failed\n");
        return 0;
    }

    if (secr_set_header(2, cnum, 0, buffer) == 0) {'''
header = header.replace(needle, insert, 1)
text = text[:start] + header + text[end:]

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

print("PS2SDK SECRMAN 1.4 dev.8 explicit-auth/session instrumentation applied")
