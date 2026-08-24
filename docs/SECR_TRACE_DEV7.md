# dev.7 one-pass SECR trace protocol

## Goal

Determine whether the per-transaction variability already exists in the MechaCon pre-CardAuth key material returned by SCMD `0x94..0x97`, or whether the observed variability is specific to SCMD `0x98` / ICVPS2.

## Why one-pass instrumentation

Calling `0x94..0x98` a second time would advance or query MechaCon state after the real KELF transaction and would not describe the values that produced the successful run. dev.7 therefore instruments the existing PS2SDK SECRMAN 1.4 path instead of replaying commands.

For normal `SecrDownloadGetKbit()` and `SecrDownloadGetKc()` calls, the instrumented SECRMAN copies the already-returned pre-CardAuth 16-byte value into unused space at RPC offset `0x100` before `card_encrypt()` mutates the ordinary Kbit/Kc buffer. The normal F2/50..53 sequence then continues unchanged.

The EE SIF wrapper captures:

- `0x94 + 0x95` pre-Kbit, 16 bytes;
- `0x96 + 0x97` pre-Kc, 16 bytes;
- final card-wrapped Kbit, 16 bytes;
- final card-wrapped Kc, 16 bytes;
- the ordinary single `0x98` ICVPS2 result, 8 bytes.

No additional SCMD or CardAuth command is issued for observability.

## Evidence files

Each RUN directory may contain:

- `mecha-pre-kbit.bin`
- `mecha-pre-kc.bin`
- `final-kbit.bin`
- `final-kc.bin`
- `icvps2-trace.bin`
- `secr-trace.txt`

The existing `probe.json`, `processed-header.bin`, `bit-table.bin` and other evidence remain unchanged.

## First experiment

Use the hardware-validated ICV-enabled Candidate A byte-for-byte:

`SHA-256 e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`

Use the already-characterized `8MB-MG-SONY-1` card in `mc0` and perform three independent cold-boot runs using only `Native control - mc0`.

Do not use `Native + one ICV read`: Candidate A itself has `Uses_ICVPS2=1`, so native-control mode already executes exactly one normal `0x98` after Kbit/Kc.

## Interpretation

### Case A: pre-Kbit/pre-Kc vary between cold boots

If `mecha-pre-kbit.bin` and/or `mecha-pre-kc.bin` change while final Kbit/Kc remain stable for the same physical card, CardAuth is deterministically collapsing session-dependent MechaCon material into the stable card-bound KELF keys. ICVPS2 can then be compared against the same session material.

### Case B: pre-Kbit/pre-Kc remain stable while ICVPS2 changes

This separates `0x98` from the Kbit/Kc preparation path and strongly supports ICVPS2 being an independent transaction/transcript authenticator rather than a direct derivative of the pre-key values.

### Case C: all pre-key and ICV values change

The next step is to instrument the preceding authentication/challenge exchange and correlate CardNonce/MechaChallenge material with the `0x94..0x98` results.

The physical mc0/mc1 port has already been eliminated as a final Kbit/Kc variable for the tested Sony card.
