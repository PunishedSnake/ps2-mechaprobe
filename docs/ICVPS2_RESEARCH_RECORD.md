# ICVPS2 / MagicGate KELF research record

This document is the detailed experiment record for the `ps2-mechaprobe` reverse-engineering series. It complements the shorter `ICVPS2_EXPERIMENT.md` narrative and intentionally records exact hardware-observed values needed for reproduction and comparison.

## Evidence labels

The following labels are used throughout this record:

- **CONFIRMED**: current source, byte-level verification, or reproduction on real PlayStation 2 hardware.
- **CURRENT IMPLEMENTATION**: behavior of the pinned PS2SDK/toolchain revision used by the probe.
- **HISTORICAL**: behavior or source from older FreeMcBoot / scene code.
- **INFERENCE**: a logical explanation consistent with the evidence but not yet uniquely proven.
- **TEST HYPOTHESIS**: a claim the next controlled hardware experiment is designed to distinguish.

Real-hardware results take precedence over emulator timing or historical scene anecdotes.

## Hardware/software baseline

Primary hardware used in this experiment series:

- console: Japanese retail FAT PS2, operator-reported SCPH-50000;
- ROMVER captured by the probe: `0170JC20030206` (the current JSON escaping path may display trailing non-printables as `??`);
- MechaCon `sceCdMV` raw: `050100ff`;
- parsed MechaCon version: `05.01`;
- MagicGate / MechaCon region: Japan;
- PS2 system type reported by the MV result: PS2;
- PS2SDK security source pinned for instrumented SECRMAN/SECRSIF: `a13b5971ec0e39c7ba8b8559b80a4e81c8425352`;
- build container: `ps2dev/ps2dev:v2.0.0`;
- memory-card slot: slot 0 on the selected physical port;
- KELF input path: `mass:/PS2DF-MECHA/input.kelf`.

Cards used in the controlled series:

- `8MB-MG-SONY-1`: Sony 8 MB MagicGate card;
- `8MB-MG-SONY-2`: second Sony 8 MB MagicGate card, also used as an FMCB card during development;
- `64MB-MG-NOTSONY`: 64 MB MagicGate-capable card without Sony branding.

The dev.7 three-cold-boot series was initially labelled SONY-1 in the test plan. The operator later confirmed that the physical card actually used for those three runs was **SONY-2**, because it had not been swapped back after the preceding mc0/mc1 slot test. This correction is authoritative for the dev.7 dataset.

---

# 1. Reference KELF and initial failures

The source template is FreeMcBoot Installer 1.966 `FMCB.XLF`:

- size: `78736` bytes;
- SHA-256: `edd1931816ca22d96835cea17ff14846b3fecb5a6ea9b346a3b079c4394508d9`;
- flags: `0x022c`;
- KELF header size: `0x0080`;
- base-header BIT count: `0`;
- application type: `1`;
- MagicGate zones: `0xff`.

## dev.2 forced-flag experiment

**CONFIRMED**: changing only `flags 0x022c -> 0x022e` in RAM caused `SecrDownloadHeader` to fail (`-120`). The saved original input remained byte-identical to the reference file, and the processed header differed at the forced flag only.

Later KELF-crypto analysis explains why: the flags are covered by the KELF header signature. Flipping the bit after signing produces an invalid signed header rather than a valid ICV-enabled KELF.

## dev.3 native control / native + late ICV read

Both native transactions accepted the original header, returned a valid two-entry BIT and processed the first 32-byte encrypted block, but failed at `SecrDownloadGetKbit` (`-124`). The returned BIT described the payload exactly:

- BIT header size: `0x80`;
- block count: `2`;
- block 0: size `0x20`, flags `0x3`, signature `517193d4c6b693ea`;
- block 1: size `0x132f0`, flags `0x0`;
- total block sizes: exactly `input_size - header_size`.

## dev.4 libmc initialization

`mcInit(MC_TYPE_XMC)` was added as one controlled variable. The failure remained `SecrDownloadGetKbit / -124`, proving that missing EE-side libmc initialization was not the sole cause.

## dev.5 physical SECR port bridge

The decisive fix was translating libmc logical memory-card ports to the physical SIO2 channels expected by the SECRMAN card-auth path:

```text
logical mc0 -> physical SECR/SIO2 channel 2
logical mc1 -> physical SECR/SIO2 channel 3
```

After this bridge, the native reference KELF completed Header -> Blocks -> Kbit -> Kc on real hardware.

**CONFIRMED**: the port mapping is a transport requirement. Later mc0/mc1 comparison with the same physical card showed that the physical port does not alter the final card-wrapped Kbit/Kc once the correct channel is selected.

---

# 2. Candidate A: first validated Uses_ICVPS2 KELF

Candidate A was reconstructed cryptographically instead of changing the flag after signing.

Layout:

```text
0x00..0x1f  base KELF header
0x20..0x27  header signature
0x28..0x37  encrypted Kbit
0x38..0x47  encrypted Kc
0x48..0x6f  encrypted BIT table (0x28 bytes)
0x70..0x77  BIT signature
0x78..0x7f  root signature
0x80..0x87  ICVPS2 slot
0x88..      payload
```

Properties:

- flags: `0x022e`;
- KELF header size: `0x0088`;
- BIT header-size field: `0x0088`;
- ICVPS2 placeholder: zero-filled `0x80..0x87`;
- source payload plaintext: unchanged from the original FMCB template;
- Candidate A SHA-256: `e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`.

Offline KELF crypto verification produced:

- header signature: `84c3ebbfcf59eb2b`;
- BIT signature: `22a235f5a3ad7123`;
- root signature: `6e45eb67f7446ad0`;
- signed block 0 signature: `517193d4c6b693ea`.

Rebuilding Candidate A from the decrypted structures reproduces Candidate A byte-for-byte.

**CONFIRMED on real hardware**: Candidate A is accepted by the retail MechaCon and completes the normal native path including exactly one `SecrDownloadGetICVPS2()` / SCMD `0x98`. The returned BIT reports header size `0x88`, and the returned eight-byte ICV is stored at `0x80..0x87`.

Candidate B, which tested reuse of the old root-signature slot without extending the header, is retained only as historical hypothesis material. Candidate A already validated the extended-header model.

---

# 3. Three-card Candidate-A dataset

Nine controlled Candidate-A runs were completed across three MagicGate-capable cards, three cold boots per card.

## 8MB-MG-SONY-1

Stable final card-wrapped values:

```text
Kbit = cf68bb2e34c67390cf68bb2e34c67390
Kc   = cf68bb2e34c67390cf68bb2e34c67390
```

Observed ICVPS2 values across three cold boots:

```text
40c1617971870799
93c8f0df6432b48c
2f9f94a51a3ce6e8
```

An additional same-card mc1 port test completed successfully with the same final Kbit/Kc and a transaction-specific ICVPS2 value recorded as:

```text
ab14f93c8912ff65
```

## 8MB-MG-SONY-2

Stable final card-wrapped values:

```text
Kbit = 0e51b4a7bd24e0840e51b4a7bd24e084
Kc   = 0e51b4a7bd24e0840e51b4a7bd24e084
```

Observed ICVPS2 values in the initial three-card series:

```text
36bb1bfc7575a2a7
283557458a6f9b46
952eacdd146b16e5
```

## 64MB-MG-NOTSONY

Stable final card-wrapped values:

```text
Kbit = 160a03515708a1e424f64cdcc57f6c02
Kc   = 14d9895d4a9fd9833b81b6bd30f92c74
```

Observed ICVPS2 values:

```text
286eae383829b647
35c7797b24ecafc5
e263c4fe018ba791
```

## Result of the card comparison

**CONFIRMED**:

- final Kbit/Kc are stable across repeated cold boots for the same physical card;
- final Kbit/Kc differ between the tested physical cards;
- the unbranded 64 MB card does not show the repeated-half / `Kbit == Kc` pattern observed with both tested Sony cards;
- ICVPS2 changes between transactions even when console, KELF bytes and physical card remain unchanged;
- the original nine-card-series ICV values were all distinct.

**INFERENCE**: the final Kbit/Kc values represent a card-bound/storage-domain representation of the KELF content keys, while ICVPS2 depends on additional MechaCon transaction/session state.

The evidence rules out ICVPS2 being merely a fixed KELF hash, fixed console value, fixed card identifier or fixed `(KELF, card)` value.

---

# 4. Physical mc0/mc1 port experiment

The same SONY-1 card was tested through mc0 and mc1 with byte-identical Candidate A.

**CONFIRMED**:

- the final Kbit/Kc were identical between ports;
- the BIT was identical;
- processed-header differences were confined to the `0x80..0x87` ICV slot;
- logical port 0 maps to physical SECR/SIO2 2 and logical port 1 maps to physical 3;
- once the transport mapping is correct, the physical port is not a determinant of final card-wrapped Kbit/Kc for the tested card/KELF.

No further multi-card mc1 series is currently required for this question.

---

# 5. dev.7: pre-card key capture

Dev.7 instrumented the existing successful SECRMAN path without replaying SCMD or card commands. It captured the values returned by MechaCon `0x94/0x95` and `0x96/0x97` before `card_encrypt()` changed the buffers.

The physical card used for this three-run dataset was **SONY-2**. The original plan incorrectly named SONY-1; the operator later corrected the physical-card identity.

Across three cold boots, Candidate A remained byte-identical and final Kbit/Kc stayed at the SONY-2 card value:

```text
0e51b4a7bd24e0840e51b4a7bd24e084
```

Observed pre-card values and ICVPS2:

| run | `0x94+0x95` pre-Kbit | `0x96+0x97` pre-Kc | ICVPS2 |
|---|---|---|---|
| A | `f50a1deeebf5d2f9` x2 | `f50a1deeebf5d2f9` x2 | `ab14f93c8912ff65` |
| B | `7cfed3126163941a` x2 | `7cfed3126163941a` x2 | `bf506b74da126d61` |
| C | `076031212a32de15` x2 | `076031212a32de15` x2 | `1cbff71f0b95a5aa` |

**CONFIRMED**: the variability already exists before the card-side F2 transform. The stable final card-bound keys are therefore not evidence of a stable MechaCon pre-key value.

---

# 6. dev.8 / dev.9 negative-state experiments

These experiments are intentionally preserved because they identify state-machine hazards.

## dev.8 explicit second SecrAuthCard

Dev.8 inserted an explicit full `SecrAuthCard()` immediately before the KELF transaction. Candidate A remained byte-identical, but `SecrDownloadHeader` failed before the normal BIT/KELF flow and no successful auth transcript was captured.

**CONFIRMED**: injecting a second full card-authentication transaction changes/invalidates the state needed by the already-working download path. This was an experimental error, not a Candidate-A failure.

## dev.9 mcGetInfo/mcSync probe

Dev.9 attempted to trigger a normal MCMAN card probe before the KELF path. It failed with:

```text
sceMcResFailResetAuth = -11
```

Current PS2SDK maps this to MCMAN's auth-reset path, which uses card command `F3`.

The KELF transaction was not attempted.

### Warm Browser side effect

The operator observed that SONY-2, which contained FMCB, remained visible in the Browser filesystem after the dev.9 run but FMCB was no longer detected on the subsequent warm Browser return.

The old probe exit path called `ExecOSD` without first restoring the ROM IOP environment, so the Browser inherited the probe's instrumented SECRMAN/MCMAN/MCSERV state in addition to the failed `F3` reset-auth state.

**CONFIRMED observation**: filesystem-visible card + missing FMCB on that warm return.

**NOT CONFIRMED**: persistent modification of card filesystem data. No KELF transaction or intentional filesystem write occurred in dev.9, and the warm-return environment was confounded by the still-resident custom IOP security stack.

Dev.10 fixes the warm-return confounder by resetting the IOP before `ExecOSD`.

The full-auth / forced-reset line of experimentation is abandoned for the current KELF-binding question.

---

# 7. dev.10: direct F2/50-53 hardware trace

Dev.10 restores the successful dev.7 semantics and adds observability only around the four stock `card_encrypt()` calls. It issues no `mcGetInfo`, no `F3`, no explicit `SecrAuthCard` and no replay.

For every 8-byte Kbit/Kc half it records:

- physical SECR/SIO2 port;
- input passed into the normal F2 sequence;
- success bitmap for `F2/50`, `F2/51`, `F2/52`, `F2/53`;
- failed-command field;
- output returned by `F2/53`.

A complete successful transform has `mask=0x0f` and `failed=0x00`.

## Preserved dev.10 evidence bundle

Input archive supplied after dev.10 contained three successful cold-boot RUN directories. All three runs used:

- Candidate A SHA-256 `e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`;
- logical memory-card port `1` / slot `0`;
- physical traced SECR/SIO2 port `3`;
- MechaCon raw `050100ff`, parsed `05.01`, region Japan;
- identical returned BIT SHA-256 `600486e0f71109358b650c05adbfa974bd71bf00055291b7475628b68265d89e`.

The use of mc1 in this series was incidental, but it also reproduces the earlier conclusion that correct physical-port selection does not alter final SONY-2 card-bound Kbit/Kc.

### RUN0001

```text
RTC evidence: 2026-08-25T22:33:28
pre-Kbit: fde495c59b5f846efde495c59b5f846e
pre-Kc:   fde495c59b5f846efde495c59b5f846e

Kbit half0 F2: in=fde495c59b5f846e out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kbit half1 F2: in=fde495c59b5f846e out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half0 F2: in=fde495c59b5f846e out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half1 F2: in=fde495c59b5f846e out=0e51b4a7bd24e084 mask=0x0f failed=0x00

final Kbit: 0e51b4a7bd24e0840e51b4a7bd24e084
final Kc:   0e51b4a7bd24e0840e51b4a7bd24e084
ICVPS2:    a0584a91346c89e1
processed-header SHA-256: 584bcf1dd6a86246d80eb065a07b28b8b2dd0a898da5ee7ca6231c6413f1cd45
```

### RUN0002

```text
RTC evidence: 2026-08-25T22:34:28
pre-Kbit: a75a37c7c671238ca75a37c7c671238c
pre-Kc:   a75a37c7c671238ca75a37c7c671238c

Kbit half0 F2: in=a75a37c7c671238c out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kbit half1 F2: in=a75a37c7c671238c out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half0 F2: in=a75a37c7c671238c out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half1 F2: in=a75a37c7c671238c out=0e51b4a7bd24e084 mask=0x0f failed=0x00

final Kbit: 0e51b4a7bd24e0840e51b4a7bd24e084
final Kc:   0e51b4a7bd24e0840e51b4a7bd24e084
ICVPS2:    6c5365e80acb26a7
processed-header SHA-256: e4480f779e242af2d4b54188757410b73095f09874722661da10cbfadd224e5b
```

### RUN0003

```text
RTC evidence: 2026-08-25T22:35:29
pre-Kbit: bc4a91cf287f4834bc4a91cf287f4834
pre-Kc:   bc4a91cf287f4834bc4a91cf287f4834

Kbit half0 F2: in=bc4a91cf287f4834 out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kbit half1 F2: in=bc4a91cf287f4834 out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half0 F2: in=bc4a91cf287f4834 out=0e51b4a7bd24e084 mask=0x0f failed=0x00
Kc   half1 F2: in=bc4a91cf287f4834 out=0e51b4a7bd24e084 mask=0x0f failed=0x00

final Kbit: 0e51b4a7bd24e0840e51b4a7bd24e084
final Kc:   0e51b4a7bd24e0840e51b4a7bd24e084
ICVPS2:    933a37a8666e4731
processed-header SHA-256: fb4222d7766744f40facc85e39d9d68eb854b81c49ec9e634863539353599aaa
```

Comparing the three processed headers shows exactly eight differing byte offsets:

```text
0x80..0x87
```

Those are the ICVPS2 bytes. All other 128 bytes are identical across the three dev.10 runs.

---

# 8. What the pre-key values mean

Offline decryption of Candidate A using the public KELF algorithm yields:

```text
plaintext Kbit = 39393939393939393939393939393939
plaintext Kc   = 39393939393939393939393939393939
```

Therefore all four 8-byte content-key halves are the same plaintext block:

```text
3939393939393939
```

Pinned public `ps3mca_tool` source (`israpps/ps3mca_tool`, commit `8115951ff95dc2220965e72ebdbaf2da6c2a4853`, `src/mecha_emu.c`) implements the matching model:

1. decrypt Kbit/Kc from the KELF header;
2. encrypt each 8-byte half under the current 8-byte MagicGate `SessionKey` with DES-CBC and zero IV;
3. send that session-wrapped value to the card for final binding.

For a one-block 8-byte input, DES-CBC with zero IV reduces to one DES block under the session key.

The dev.10 trace therefore provides real-hardware vectors consistent with:

```text
RUN0001: DES(SessionKey_1, 3939393939393939) = fde495c59b5f846e
RUN0002: DES(SessionKey_2, 3939393939393939) = a75a37c7c671238c
RUN0003: DES(SessionKey_3, 3939393939393939) = bc4a91cf287f4834
```

and then the SONY-2 card-side F2 path maps each session-wrapped value to the same stable card-bound result:

```text
0e51b4a7bd24e084
```

**CONFIRMED**: the input to F2 changes between cold boots and the F2 output remains stable for the same card/plaintext content key.

**INFERENCE**: F2 performs a rewrapping from the current session-key domain into a card/storage-bound domain. This matches the public MagicGate emulator model, but the internal card storage-key algorithm has not been independently reconstructed from these traces alone.

**CONFIRMED**: ICVPS2 remains transaction-variable in the same runs while final card-bound Kbit/Kc remain stable.

---

# 9. Candidate C: next controlled vector

Candidate A wastes three of the four potential equations because all plaintext Kbit/Kc halves are identical. Candidate C changes only the plaintext Kbit/Kc values while preserving the validated ICV-enabled KELF structure and plaintext payload.

Candidate C plaintext values:

```text
Kbit half0 = 0011223344556677
Kbit half1 = 8899aabbccddeeff
Kc   half0 = fedcba9876543210
Kc   half1 = 0f1e2d3c4b5a6978
```

Combined:

```text
Kbit = 00112233445566778899aabbccddeeff
Kc   = fedcba98765432100f1e2d3c4b5a6978
```

Candidate C properties:

- size: `78744` bytes;
- flags: `0x022e`;
- header size: `0x0088`;
- header signature: `84c3ebbfcf59eb2b`;
- BIT signature: `a3d4fe1e0ce75c0f`;
- root signature: `a7b51a94f33f8e4e`;
- plaintext payload SHA-256: `d7922c6eb6cce23c05808a77ffe0c2c58758414d707da4aebd85a7fabec9d69d`;
- Candidate C SHA-256: `87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d`.

The generator was self-tested by first regenerating Candidate A with the original plaintext Kbit/Kc. The generated file was byte-for-byte identical to hardware-validated Candidate A and reproduced SHA-256 `e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`.

Candidate C then passed full offline verification:

- stored encrypted Kbit decrypts to the four chosen Kbit bytes;
- stored encrypted Kc decrypts to the chosen Kc bytes;
- encrypted BIT decrypts to the same `0x88`, two-block BIT structure;
- BIT signature recomputes exactly;
- root signature recomputes exactly;
- signed block 0 decrypts to the same plaintext ELF bytes as Candidate A;
- signed block signature recomputes exactly;
- full plaintext payload is byte-identical to Candidate A;
- the ICV slot is zero before hardware binding.

The repository generator does not contain key material. It requires a user-supplied kelftool-compatible `PS2KEYS.dat`.

---

# 10. Candidate C test plan

Use the unchanged, hardware-successful dev.10 probe. Do **not** change the ELF for the first Candidate C experiment; the only controlled variable should be the KELF.

## Phase C1: validate the vector on SONY-2

Use:

```text
card: 8MB-MG-SONY-2
port: mc0
mode: Native F2 trace - mc0
input SHA-256: 87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d
```

Perform three independent cold boots. Full power off between runs.

Stop after the first run if `SecrDownloadHeader`, BIT, block, Kbit, Kc or ICV fails. Do not collect repeated identical failures before analysing the first evidence bundle.

If all three succeed, compare for each transaction:

```text
P0 -> pre-Kbit half0 -> F2 output0
P1 -> pre-Kbit half1 -> F2 output1
P2 -> pre-Kc   half0 -> F2 output2
P3 -> pre-Kc   half1 -> F2 output3
ICVPS2
```

### TEST HYPOTHESIS C1

If the public session-encryption model is correct, all four pre-card values within a single run should be the four distinct DES encryptions of P0..P3 under one session key. They should change between cold boots as the session changes.

If the card-side rewrapping model is correct, the four final F2 outputs should be stable across cold boots for the same card and plaintext content-key blocks, even though their session-wrapped inputs change.

## Phase C2: change only the physical card

After C1 succeeds, repeat three cold boots each with:

```text
8MB-MG-SONY-1
64MB-MG-NOTSONY
```

Keep Candidate C, mc0 and dev.10 unchanged.

### TEST HYPOTHESIS C2

For the same four plaintext content-key blocks:

- pre-card values are expected to follow session state rather than physical-card identity;
- final F2 outputs should remain stable per card but differ across cards if the storage-domain wrapping is card-specific;
- ICVPS2 should continue to be treated as transaction/session-variable until a stronger dependency is demonstrated.

## What not to do

For this experiment:

- do not call the legacy `Native + one ICV read` mode;
- do not replay `0x94..0x98`;
- do not call `mcGetInfo/mcSync` as a session-reset mechanism;
- do not inject `F3`;
- do not inject a second `SecrAuthCard`;
- do not mix mc0/mc1 within the primary C1/C2 dataset;
- do not write/reinstall FMCB on a card merely because a warm-return state looks wrong; preserve evidence first.

---

# 11. Current model and remaining unknowns

## CONFIRMED

```text
KELF plaintext content keys
        |
        v
MechaCon 0x94..0x97 produces transaction-variable pre-card key material
        |
        v
stock card F2/50 -> 51 -> 52 -> 53
        |
        v
stable card-bound Kbit/Kc for the same card/content key
```

In parallel, a valid ICV-enabled KELF causes a normal single SCMD `0x98` to return an eight-byte ICVPS2 value that changes between transactions.

## CURRENT IMPLEMENTATION

Pinned PS2SDK SECRMAN 1.4 uses `card_encrypt()` with the F2/50-53 sequence for the Kbit/Kc download-binding path. The probe instruments this path in-band rather than replaying it.

## INFERENCE

The hardware observations and `ps3mca_tool` model are consistent with:

```text
plaintext content key
  -> DES under MagicGate SessionKey
  -> session-wrapped content key
  -> card-side F2 rewrap
  -> card/storage-wrapped content key
```

The exact internal storage-key derivation inside a retail card is not proven by the current dataset.

## UNKNOWN / NEXT RESEARCH

- exact mathematical dependency of ICVPS2 on KELF state and the active MagicGate session;
- whether ICVPS2 can be reproduced entirely from known session/key transcript data;
- exact source of the active session state inherited by the successful KELF path after boot/IOP reset;
- whether card family/implementation affects only storage wrapping or other hidden transcript state;
- cross-console and cross-MechaCon-revision behavior;
- cross-region behavior;
- whether the complete pipeline can be reproduced offline for universal HDD-OSD/FHDB/PSBBN provisioning.

PCSX2 may be used for correctness/inspection, but subtle state and performance claims in this project require confirmation on real hardware.

---

# 12. Source anchors

Pinned/current source anchors used by the experiment:

- PS2SDK security snapshot: `https://github.com/ps2dev/ps2sdk/tree/a13b5971ec0e39c7ba8b8559b80a4e81c8425352/iop/security`
- PS2SDK `libsecr`: `https://github.com/ps2dev/ps2sdk/blob/a13b5971ec0e39c7ba8b8559b80a4e81c8425352/ee/rpc/secr/src/libsecr.c`
- PS2SDK `SECRMAN`: `https://github.com/ps2dev/ps2sdk/blob/a13b5971ec0e39c7ba8b8559b80a4e81c8425352/iop/security/secrman/src/secrman.c`
- PS2SDK CardAuth helpers: `https://github.com/ps2dev/ps2sdk/blob/a13b5971ec0e39c7ba8b8559b80a4e81c8425352/iop/security/secrman/src/CardAuth.c`
- `ps3mca_tool` MechaCon emulator: `https://github.com/israpps/ps3mca_tool/blob/8115951ff95dc2220965e72ebdbaf2da6c2a4853/src/mecha_emu.c`
- kelftool algorithm reference: `https://github.com/israpps/kelftool`

Historical FreeMcBoot source is relevant for provenance of the known `0x98` / ICVPS2 read path, but historical comments are not used as authority over current source or real-hardware reproduction.
