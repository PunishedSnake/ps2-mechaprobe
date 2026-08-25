# dev.7 one-pass SECR trace protocol and result

## Goal

Determine whether per-transaction variability already exists in the MechaCon pre-card key material returned by SCMD `0x94..0x97`, or whether the observed variability is specific to SCMD `0x98` / ICVPS2.

## Why one-pass instrumentation

Calling `0x94..0x98` a second time would query or advance MechaCon state after the real KELF transaction and would not describe the values that produced the successful run. dev.7 therefore instruments the existing PS2SDK SECRMAN 1.4 path instead of replaying commands.

For normal `SecrDownloadGetKbit()` and `SecrDownloadGetKc()` calls, the instrumented SECRMAN copies the already-returned pre-card 16-byte value into unused RPC space before `card_encrypt()` mutates the ordinary Kbit/Kc buffer. The normal F2/50..53 sequence then continues unchanged.

The EE SIF wrapper captures:

- `0x94 + 0x95` pre-Kbit, 16 bytes;
- `0x96 + 0x97` pre-Kc, 16 bytes;
- final card-wrapped Kbit, 16 bytes;
- final card-wrapped Kc, 16 bytes;
- the ordinary single `0x98` ICVPS2 result, 8 bytes.

No additional SCMD or card command is issued for observability.

## Evidence files

Each successful RUN may contain:

- `mecha-pre-kbit.bin`
- `mecha-pre-kc.bin`
- `final-kbit.bin`
- `final-kc.bin`
- `icvps2-trace.bin`
- `secr-trace.txt`

The existing `probe.json`, `processed-header.bin`, `bit-table.bin` and other evidence remain unchanged.

## Input

The hardware-validated ICV-enabled Candidate A was used byte-for-byte:

`SHA-256 e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`

Candidate A itself has `Uses_ICVPS2=1`, so native-control mode executes exactly one normal `0x98` after Kbit/Kc. The legacy `Native + one ICV read` mode was not used for this experiment.

## Card-identity correction

The initial test plan said to use `8MB-MG-SONY-1` in mc0. After the runs, the operator confirmed that the physical card had not been swapped back after the preceding slot experiment and the three dev.7 cold boots were actually performed with **`8MB-MG-SONY-2`**.

This correction is authoritative for the dev.7 dataset.

## Real-hardware result

All three cold boots completed the native Candidate-A transaction.

Final card-wrapped material remained stable at the previously observed SONY-2 value:

```text
Kbit = 0e51b4a7bd24e0840e51b4a7bd24e084
Kc   = 0e51b4a7bd24e0840e51b4a7bd24e084
```

The pre-card values changed between cold boots:

| run | `0x94+0x95` pre-Kbit | `0x96+0x97` pre-Kc | ICVPS2 |
|---|---|---|---|
| A | `f50a1deeebf5d2f9` x2 | `f50a1deeebf5d2f9` x2 | `ab14f93c8912ff65` |
| B | `7cfed3126163941a` x2 | `7cfed3126163941a` x2 | `bf506b74da126d61` |
| C | `076031212a32de15` x2 | `076031212a32de15` x2 | `1cbff71f0b95a5aa` |

## Interpretation

**CONFIRMED**: per-transaction variability already exists at MechaCon `0x94..0x97`, before the card-side F2 transform.

**CONFIRMED**: despite the changing pre-card values, final Kbit/Kc remain stable for the same physical card and Candidate A.

**CONFIRMED**: ICVPS2 also changes between the transactions.

Candidate A decrypts to four identical 8-byte plaintext content-key blocks (`3939393939393939`). Public `ps3mca_tool` source implements the matching model in which each plaintext Kbit/Kc half is DES-encrypted under the active MagicGate session key before card-side binding. Therefore the repeated halves inside each run are expected for Candidate A and are not evidence of a trace bug.

Dev.10 subsequently traced the stock F2/50 -> F2/51 -> F2/52 -> F2/53 transforms directly and confirmed that session-variable inputs are converted into stable SONY-2 card-bound outputs.

The physical mc0/mc1 port had already been eliminated as a final Kbit/Kc variable for the tested Sony card. The next controlled vector is Candidate C, which uses four deliberately distinct plaintext Kbit/Kc halves so one transaction yields four independent equations under the same session.

See `ICVPS2_RESEARCH_RECORD.md` for the complete experiment timeline and exact dev.10 vectors.
