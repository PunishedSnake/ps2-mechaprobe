#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned PS2SDK SECRMAN 1.4 for dev.11.

The hardware-successful dev.10 transaction semantics are preserved exactly:
- no mcGetInfo probe,
- no F3 auth reset,
- no extra SecrAuthCard,
- no MechaCon/CardAuth replay.

In-band observability:
- 0x94/0x95 and 0x96/0x97 pre-card key material at RPC offset 0x100,
- the four stock F2/50-53 card_encrypt transforms at RPC offset 0x120,
- the most recent naturally occurring *successful* SecrAuthCard transcript, if
  one happens after this instrumented SECRMAN is loaded, at offset 0x180 of the
  ordinary successful ICVPS2 RPC response.

The auth transcript is never reset by the KELF path. If no natural successful
SecrAuthCard occurs, the auth response area remains zero and the normal KELF
transaction is otherwise unchanged.
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
SECRMAN = ROOT / "src" / "secrman.c"
PREKEY_RELATIVE_OFFSET = 0xF8   # kbit/kc starts RPC base+8 => base+0x100
F2TRACE_RELATIVE_OFFSET = 0x118 # kbit/kc starts RPC base+8 => base+0x120
AUTH_RELATIVE_OFFSET = 0x180    # icvps2 starts at RPC base


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


def find_definition(text: str, signature_start: str):
    needle = signature_start + "\n{"
    start = text.find(needle)
    if start < 0:
        raise SystemExit(f"missing function definition: {signature_start}")
    brace = start + len(signature_start) + 1
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
    raise SystemExit(f"unterminated function definition: {signature_start}")


def replace_function(text: str, signature_start: str, replacement: str):
    start, end = find_function(text, signature_start)
    return text[:start] + replacement + text[end:]


def replace_definition(text: str, signature_start: str, replacement: str):
    start, end = find_definition(text, signature_start)
    return text[:start] + replacement + text[end:]


text = SECRMAN.read_text()
marker = "extern struct irx_export_table _exp_secrman;\n"
if marker not in text:
    raise SystemExit("missing SECRMAN export marker")

helpers = r'''

/* dev.11 passive F2 trace + last naturally successful SecrAuthCard transcript.
   Wire formats are byte-based so EE parsing does not depend on IOP padding. */
#define MG_F2_TRACE_SIZE 56
#define MG_F2_RECORD_SIZE 24
#define MG_AUTH_TRACE_SIZE 80
static unsigned char MgF2Trace[MG_F2_TRACE_SIZE];
static unsigned char MgAuthTrace[MG_AUTH_TRACE_SIZE];
static int MgAuthTraceValid;

static void MgF2TraceReset(unsigned char kind)
{
    memset(MgF2Trace, 0, sizeof(MgF2Trace));
    MgF2Trace[0] = 'M';
    MgF2Trace[1] = 'G';
    MgF2Trace[2] = 'F';
    MgF2Trace[3] = '2';
    MgF2Trace[4] = 1;
    MgF2Trace[5] = 0;
    MgF2Trace[6] = kind;
}

static int MgF2TraceBegin(int port, int slot, const void *input)
{
    int index;
    unsigned char *record;

    index = MgF2Trace[5];
    if (index >= 2)
        return -1;

    MgF2Trace[5] = (unsigned char)(index + 1);
    record = &MgF2Trace[8 + index * MG_F2_RECORD_SIZE];
    memset(record, 0, MG_F2_RECORD_SIZE);
    record[0] = (unsigned char)port;
    record[1] = (unsigned char)slot;
    record[2] = 0;
    record[3] = 0xff;
    memcpy(&record[4], input, 8);
    return index;
}

static void MgF2TraceStep(int index, unsigned char bit, unsigned char command, int success)
{
    unsigned char *record;

    if (index < 0 || index >= 2)
        return;
    record = &MgF2Trace[8 + index * MG_F2_RECORD_SIZE];
    if (success)
        record[2] |= bit;
    else
        record[3] = command;
}

static void MgF2TraceFinish(int index, const void *output)
{
    unsigned char *record;

    if (index < 0 || index >= 2)
        return;
    record = &MgF2Trace[8 + index * MG_F2_RECORD_SIZE];
    memcpy(&record[12], output, 8);
    if ((record[2] & 0x0f) == 0x0f)
        record[3] = 0;
}

static void MgF2TraceCopy(void *dest)
{
    memcpy(dest, MgF2Trace, sizeof(MgF2Trace));
}

static void MgAuthTraceCapture(int port, int slot, int cnum,
                               const void *CardIV, const void *CardMaterial,
                               const void *CardNonce,
                               const void *MechaChallenge1,
                               const void *MechaChallenge2,
                               const void *MechaChallenge3,
                               const void *CardResponse1,
                               const void *CardResponse2,
                               const void *CardResponse3)
{
    memset(MgAuthTrace, 0, sizeof(MgAuthTrace));
    MgAuthTrace[0] = 'M';
    MgAuthTrace[1] = 'G';
    MgAuthTrace[2] = 'A';
    MgAuthTrace[3] = '1';
    MgAuthTrace[4] = 1;
    MgAuthTrace[5] = (unsigned char)cnum;
    MgAuthTrace[6] = (unsigned char)port;
    MgAuthTrace[7] = (unsigned char)slot;
    memcpy(&MgAuthTrace[8],  CardIV, 8);
    memcpy(&MgAuthTrace[16], CardMaterial, 8);
    memcpy(&MgAuthTrace[24], CardNonce, 8);
    memcpy(&MgAuthTrace[32], MechaChallenge1, 8);
    memcpy(&MgAuthTrace[40], MechaChallenge2, 8);
    memcpy(&MgAuthTrace[48], MechaChallenge3, 8);
    memcpy(&MgAuthTrace[56], CardResponse1, 8);
    memcpy(&MgAuthTrace[64], CardResponse2, 8);
    memcpy(&MgAuthTrace[72], CardResponse3, 8);
    MgAuthTraceValid = 1;
}
'''
text = text.replace(marker, marker + helpers, 1)

# Capture only the normal successful SecrAuthCard path. Never invoke it here.
auth_start, auth_end = find_function(text, "int SecrAuthCard(")
auth = text[auth_start:auth_end]
needle = '    _printf("mechacon auth 0x88\\n");\n\n    return 1;'
if needle not in auth:
    needle = '    _printf("mechacon auth 0x88!\\n");\n\n    return 1;'
if needle not in auth:
    raise SystemExit("SecrAuthCard success marker not found")
capture = r'''    _printf("mechacon auth 0x88\n");

    MgAuthTraceCapture(port, slot, cnum,
                       CardIV, CardMaterial, CardNonce,
                       MechaChallenge1, MechaChallenge2, MechaChallenge3,
                       CardResponse1, CardResponse2, CardResponse3);
    return 1;'''
auth = auth.replace(needle, capture, 1)
text = text[:auth_start] + auth + text[auth_end:]

new_card_encrypt = r'''static int card_encrypt(int port, int slot, void *buffer)
{
    int trace_index;

    trace_index = MgF2TraceBegin(port, slot, buffer);

    if (GetMcCommandHandler() == NULL) {
        MgF2TraceStep(trace_index, 0, 0xfe, 0);
        return 0;
    }

    if (card_auth(port, slot, 0xF2, 0x50) == 0) {
        MgF2TraceStep(trace_index, 0x01, 0x50, 0);
        return 0;
    }
    MgF2TraceStep(trace_index, 0x01, 0x50, 1);

    if (card_auth_write(port, slot, buffer, 0xF2, 0x51) == 0) {
        MgF2TraceStep(trace_index, 0x02, 0x51, 0);
        return 0;
    }
    MgF2TraceStep(trace_index, 0x02, 0x51, 1);

    if (card_auth(port, slot, 0xF2, 0x52) == 0) {
        MgF2TraceStep(trace_index, 0x04, 0x52, 0);
        return 0;
    }
    MgF2TraceStep(trace_index, 0x04, 0x52, 1);

    if (card_auth_read(port, slot, buffer, 0xF2, 0x53) == 0) {
        MgF2TraceStep(trace_index, 0x08, 0x53, 0);
        return 0;
    }
    MgF2TraceStep(trace_index, 0x08, 0x53, 1);
    MgF2TraceFinish(trace_index, buffer);
    return 1;
}'''

new_kbit = f'''int SecrDownloadGetKbit(int port, int slot, void *kbit)
{{
    int result = 0;
    MgF2TraceReset('K');

    if (scePreEncryptKbit(kbit) == 0)
        goto end;
    memcpy((void *)((unsigned char *)kbit + 0x{PREKEY_RELATIVE_OFFSET:03x}), kbit, 16);
    if (card_encrypt(port, slot, kbit) == 0)
        goto end;
    if (card_encrypt(port, slot, (void *)((unsigned char *)kbit + 8)) == 0)
        goto end;
    result = 1;
end:
    MgF2TraceCopy((void *)((unsigned char *)kbit + 0x{F2TRACE_RELATIVE_OFFSET:03x}));
    return result;
}}'''

new_kc = f'''int SecrDownloadGetKc(int port, int slot, void *kc)
{{
    int result = 0;
    MgF2TraceReset('C');

    if (scePreEncryptKc(kc) == 0)
        goto end;
    memcpy((void *)((unsigned char *)kc + 0x{PREKEY_RELATIVE_OFFSET:03x}), kc, 16);
    if (card_encrypt(port, slot, kc) == 0)
        goto end;
    if (card_encrypt(port, slot, (void *)((unsigned char *)kc + 8)) == 0)
        goto end;
    result = 1;
end:
    MgF2TraceCopy((void *)((unsigned char *)kc + 0x{F2TRACE_RELATIVE_OFFSET:03x}));
    return result;
}}'''

new_icv = f'''int SecrDownloadGetICVPS2(void *icvps2)
{{
    int result;

    result = func_00000d14(icvps2);
    memset((void *)((unsigned char *)icvps2 + 0x{AUTH_RELATIVE_OFFSET:03x}), 0, MG_AUTH_TRACE_SIZE);
    if (result != 0 && MgAuthTraceValid)
        memcpy((void *)((unsigned char *)icvps2 + 0x{AUTH_RELATIVE_OFFSET:03x}), MgAuthTrace, MG_AUTH_TRACE_SIZE);
    return result;
}}'''

text = replace_definition(text, "static int card_encrypt(int port, int slot, void *buffer)", new_card_encrypt)
text = replace_function(text, "int SecrDownloadGetKbit(", new_kbit)
text = replace_function(text, "int SecrDownloadGetKc(", new_kc)
text = replace_function(text, "int SecrDownloadGetICVPS2(", new_icv)

SECRMAN.write_text(text)
print("PS2SDK SECRMAN 1.4 dev.11 passive F2 + natural-auth instrumentation applied")
