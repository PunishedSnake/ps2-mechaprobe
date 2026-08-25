# dev.11 passive natural-auth result

This document records the first real-hardware result from dev.11, whose purpose was to observe whether the instrumented SECRMAN sees a **naturally occurring successful `SecrAuthCard()`** after the probe's own IOP reset/module load, while preserving the already-successful Candidate-C KELF/F2 path.

## Evidence labels

- **CONFIRMED**: byte-level observation or real-hardware reproduction.
- **CURRENT IMPLEMENTATION**: behavior of the pinned PS2SDK/security stack used by this build.
- **INFERENCE**: logical consequence consistent with the evidence but not yet uniquely proven.
- **TEST HYPOTHESIS**: a proposed controlled experiment to distinguish remaining models.

## Supplied archive

```text
archive: RUN0002.zip
size:    82117 bytes
SHA-256: 835b498b4135b93310acf480dedfba6e454daf6239b0a2b9add97e53f16eb40a
```

The archive contains one complete `RUN0002` directory.

## Probe / input identity

```text
probe version = 0.1.0-dev.11
mode          = native_control
status        = success
stage         = complete
code          = 0
logical port  = mc0
slot          = 0
physical F2 port observed = 2
```

Candidate C was used byte-for-byte:

```text
size    = 78744 bytes
SHA-256 = 87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d
flags   = 0x022e
header  = 136 bytes / 0x88
Uses_ICVPS2 = true
```

Console evidence:

```text
ROMVER        = 0170JC20030206
MechaCon raw  = 050100ff
MechaCon      = 05.01
region        = Japan
system type   = PS2
RTC evidence  = 2026-08-26T03:58:14
```

## dev.11 instrumentation contract

Dev.11 deliberately does **not** start or reset full card authentication. It does not add:

- `mcGetInfo()`;
- `F3`;
- a second `SecrAuthCard()`;
- replayed `0x94..0x98`;
- replayed F2 commands.

It preserves the dev.10 Candidate-C transaction and only records the last successful full `SecrAuthCard()` **if the normal runtime itself invokes one after the instrumented SECRMAN is loaded**.

The decisive status file from this run is:

```text
auth_trace_present=false
```

No `auth-trace.bin`, CardIV/CardMaterial/CardNonce, MechaChallenge, or CardResponse files are present because no successful full `SecrAuthCard()` was observed in that instrumented window.

## KELF transaction remained fully successful

Despite `auth_trace_present=false`, Candidate C completed the normal path:

```text
SecrDownloadHeader
-> returned BIT
-> encrypted block
-> SecrDownloadGetKbit / 0x94+0x95
-> F2/50-53 x2
-> SecrDownloadGetKc / 0x96+0x97
-> F2/50-53 x2
-> SecrDownloadGetICVPS2 / 0x98
-> complete
```

Returned BIT SHA-256:

```text
600486e0f71109358b650c05adbfa974bd71bf00055291b7475628b68265d89e
```

The transaction reported:

```text
returned header size     = 136
returned block count     = 2
processed encrypted blocks = 1
ICV read attempted       = true
ICV read unconditional   = false
```

## Candidate-C pre-card and F2 vectors

All four normal F2 transforms completed with `mask=0x0f`, `failed=0x00`.

```text
0x94+0x95 pre-Kbit = 9f723ca2b3b6526a08f9683c3e70250f

P0 / Kbit0:
9f723ca2b3b6526a -> 4acf09a5ec5d10d4

P1 / Kbit1:
08f9683c3e70250f -> 0589da87b42f05f5

final Kbit = 4acf09a5ec5d10d40589da87b42f05f5
```

```text
0x96+0x97 pre-Kc = 3013911614de07dd63881a6a67786e1e

P2 / Kc0:
3013911614de07dd -> b559bdac76c898b0

P3 / Kc1:
63881a6a67786e1e -> 53e0c7cabbd8797b

final Kc = b559bdac76c898b053e0c7cabbd8797b
```

These final values exactly reproduce the previously established SONY-2 Candidate-C card-bound vector:

```text
P0 0011223344556677 -> 4acf09a5ec5d10d4
P1 8899aabbccddeeff -> 0589da87b42f05f5
P2 fedcba9876543210 -> b559bdac76c898b0
P3 0f1e2d3c4b5a6978 -> 53e0c7cabbd8797b
```

**CONFIRMED:** dev.11 passive-auth instrumentation did not disturb the known-good Candidate-C/F2 behavior.

## ICVPS2

The normal single SCMD `0x98` returned:

```text
782941f0adbe687d
```

`icvps2.bin`, `icvps2-trace.bin`, and the `probe.json` value agree byte-for-byte.

Processed-header SHA-256:

```text
3e67b8b0f28765fa739528363d5eef6df2cd350008d826aa70323ff9b6177fcf
```

## Decisive result

**CONFIRMED:** no successful full `SecrAuthCard()` occurred after the instrumented dev.11 SECRMAN was loaded, yet the complete Candidate-C `0x94..0x98` + F2 transaction succeeded normally.

Therefore a successful full `SecrAuthCard()` **inside this post-reset instrumented IOP window is not required immediately before the observed KELF transaction**.

This directly eliminates the dev.11 hypothesis that the session used by the successful Candidate-C transaction would be established by a naturally occurring full `SecrAuthCard()` after our own module load.

## What this does and does not prove

### CONFIRMED

- the active state needed by `0x94..0x98` exists despite no newly observed full `SecrAuthCard()` in the dev.11 window;
- the session-variable pre-card values remain available and valid;
- F2 still removes that transaction variability into the same stable SONY-2 card-bound Candidate-C vector;
- ICVPS2 is still returned normally.

### INFERENCE

The most economical model is that the session/auth state consumed by `0x94..0x98` predates the dev.11 instrumented IOP window and survives the probe's IOP reset/module replacement because at least part of that state lives outside ordinary IOP RAM, plausibly in MechaCon/card security state.

This is consistent with the observed behavior but is **not yet a direct capture of where or when that state was created**.

Do not yet state that the SessionKey is definitely created by FMCB, OSDSYS, ROM code, or one specific earlier command sequence. The current run only places a firm upper bound: it was not created by an observed successful full `SecrAuthCard()` after dev.11's SECRMAN became active.

## Next controlled discrimination

The next useful experiment should alter the **boot/session provenance** while keeping Candidate C and passive dev.11 tracing unchanged.

A useful first comparison is:

1. boot/launch environment using one MagicGate card;
2. test a different physical target card in the other port;
3. avoid probing/resetting target-card auth state before the Candidate-C transaction;
4. compare whether normal `0x94..0x98` still succeeds and whether `auth_trace_present` remains false.

A still cleaner variant is to make the target card absent during the earlier boot chain and insert it only after dev.11 reaches its menu, if the card path tolerates that controlled hot-insertion. This should be treated as a separate experiment because insertion timing itself becomes the variable.

The Candidate-E NONSONY position/state experiment remains separate from the session-origin line of research.
