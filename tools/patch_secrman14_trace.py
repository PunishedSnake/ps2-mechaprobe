#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instrument pinned PS2SDK SECRMAN 1.4 for dev.10 F2/50-53 tracing.

This deliberately restores the hardware-successful dev.7 transaction semantics:
- no mcGetInfo probe is required by the experiment,
- no F3 auth reset is issued by the probe,
- no extra SecrAuthCard is started,
- no MechaCon or card command is replayed.

Observability is added only around the four stock card_encrypt() calls used by
SecrDownloadGetKbit() and SecrDownloadGetKc(). For each 8-byte half we record:
- physical SECR port and slot,
- input bytes supplied to F2/51,
- success of F2/50, F2/51, F2/52 and F2/53,
- output bytes returned by F2/53.

The existing dev.7 raw pre-Kbit/pre-Kc capture remains at response offset 0x100.
A compact 56-byte F2 trace block is copied to unused response offset 0x120.
The normal RPC structures and KELF transaction remain otherwise unchanged.
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
SECRMAN = ROOT / "src" / "secrman.c"
PREKEY_RELATIVE_OFFSET = 0xF8   # kbit/kc field starts at RPC base + 8 => base + 0x100
F2TRACE_RELATIVE_OFFSET = 0x118 # kbit/kc field starts at RPC base + 8 => base + 0x120
F2TRACE_SIZE = 56


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

helpers = r'''

/* dev.10 passive F2/50-53 trace. Wire format is intentionally byte-based so
   the EE parser does not depend on IOP structure padding. */
#define MG_F2_TRACE_SIZE 56
#define MG_F2_RECORD_SIZE 24
static unsigned char MgF2Trace[MG_F2_TRACE_SIZE];

static void MgF2TraceReset(unsigned char kind)
{
    memset(MgF2Trace, 0, sizeof(MgF2Trace));
    MgF2Trace[0] = 'M';
    MgF2Trace[1] = 'G';
    MgF2Trace[2] = 'F';
    MgF2Trace[3] = '2';
    MgF2Trace[4] = 1;       /* trace format version */
    MgF2Trace[5] = 0;       /* record count */
    MgF2Trace[6] = kind;    /* 'K' = Kbit, 'C' = Kc */
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
    record[2] = 0;          /* successful-step bitmap: 50,51,52,53 */
    record[3] = 0xff;       /* failed command, 0 on complete success */
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
'''

text = text.replace(marker, marker + helpers, 1)

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
    int result;

    result = 0;
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
    int result;

    result = 0;
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

text = replace_function(text, "static int card_encrypt(", new_card_encrypt)
text = replace_function(text, "int SecrDownloadGetKbit(", new_kbit)
text = replace_function(text, "int SecrDownloadGetKc(", new_kc)

SECRMAN.write_text(text)
print("PS2SDK SECRMAN 1.4 dev.10 passive F2/50-53 instrumentation applied")
