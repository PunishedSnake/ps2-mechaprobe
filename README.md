# PS2 Mecha Probe

Developer-oriented PlayStation 2 homebrew for probing the MechaCon / MagicGate KELF download path and collecting reproducible ICVPS2 evidence.

The project started with a basic question: **what does MechaCon return for SCMD `0x98`, and what inputs or session state make that value change?** It has now progressed beyond a simple ICV dump into direct one-pass observation of the session-wrapped Kbit/Kc material and the stock card-side F2 binding transform.

## Current hardware-validated status

The following are confirmed on real retail PS2 hardware:

- logical memory-card ports used by EE/libsecr must be translated to the physical SIO2 channels expected by the SECRMAN card path (`mc0 -> 2`, `mc1 -> 3`);
- a cryptographically reconstructed KELF with `Uses_ICVPS2=1`, flags `0x022e`, header size `0x88` and a dedicated 8-byte ICV slot is accepted by retail MechaCon;
- the normal native path completes `Header -> encrypted block -> Kbit -> Kc -> ICVPS2` and returns a real 8-byte SCMD `0x98` value;
- ICVPS2 changes between transactions even with byte-identical KELF and the same physical card;
- final card-wrapped Kbit/Kc are stable per tested physical card but differ between cards;
- the MechaCon pre-card material returned by `0x94..0x97` changes between cold boots;
- dev.10 directly traces all four stock `F2/50 -> F2/51 -> F2/52 -> F2/53` key-binding operations without replay and shows changing session-wrapped inputs being converted into stable card-bound outputs for the same physical card;
- mc0/mc1 does not change final card-bound Kbit/Kc once the correct physical SIO2 channel is selected;
- injecting a second full `SecrAuthCard` or forcing MCMAN's `F3` reset-auth path changes the security state and is not part of the controlled KELF-binding experiment.

The full exact hardware record, including per-card Kbit/Kc, ICVPS2 values, dev.7 pre-key vectors, dev.10 F2 input/output vectors and negative-state experiments, is in:

- [`docs/ICVPS2_RESEARCH_RECORD.md`](docs/ICVPS2_RESEARCH_RECORD.md)
- [`docs/ICVPS2_EXPERIMENT.md`](docs/ICVPS2_EXPERIMENT.md)
- [`docs/SECR_TRACE_DEV7.md`](docs/SECR_TRACE_DEV7.md)

## v0.1 development scope

The probe builds its own known IOP environment with PS2SDK modules, initializes `SECRMAN` / `SECRSIF`, processes one KELF through an instrumented equivalent of `SecrDownloadFile()`, and exports the resulting evidence to USB.

Instead of calling `SecrDownloadFile()` as an opaque helper, the program deliberately performs the same public PS2SDK sequence stage by stage:

1. `SecrDownloadHeader(port, slot, ...)`
2. `SecrDownloadBlock(...)` for encrypted BIT entries
3. `SecrDownloadGetKbit(port, slot, ...)`
4. `SecrDownloadGetKc(port, slot, ...)`
5. `SecrDownloadGetICVPS2(...)` exactly once when KELF flag bit 1 says ICVPS2 is used
6. write Kbit, Kc and ICVPS2 into the processed header at the same offsets used by PS2SDK `libsecr`

Doing this explicitly lets an evidence bundle record the exact stage that failed. It also prevents a second post-transaction `0x98` call from being confused with the ICV returned inside the original KELF transaction.

The current dev.10 instrumentation additionally captures the actual stock `card_encrypt()` boundary for each 8-byte Kbit/Kc half without inserting additional card commands.

## Input

Create this directory on a FAT USB device:

```text
mass:/PS2DF-MECHA/
```

Put the KELF to test at:

```text
mass:/PS2DF-MECHA/input.kelf
```

The current development build accepts KELFs up to 16 MiB. The selected `mc0` or `mc1` must contain a compatible PS2 MagicGate memory card because the download-header and key stages use the card path.

## Evidence bundle

Each run creates the next free directory:

```text
mass:/PS2DF-MECHA/RUN0001/
mass:/PS2DF-MECHA/RUN0002/
...
```

A successful dev.10 ICV/F2-trace run may contain:

```text
probe.json
input.kelf
input.sha256
processed-header.bin
bit-table.bin
icvps2.bin
icvps2.txt
icvps2-trace.bin
mecha-pre-kbit.bin
mecha-pre-kc.bin
final-kbit.bin
final-kc.bin
f2-kbit-trace.bin
f2-kc-trace.bin
f2-kbit-half0-input.bin
f2-kbit-half0-output.bin
f2-kbit-half1-input.bin
f2-kbit-half1-output.bin
f2-kc-half0-input.bin
f2-kc-half0-output.bin
f2-kc-half1-input.bin
f2-kc-half1-output.bin
secr-trace.txt
```

`probe.json` records the input identity, KELF header flags, BIT counts, transaction progress, ROMVER, best-effort model information, raw `sceCdMV()` data, MagicGate region/version interpretation, RTC evidence and ICVPS2.

`processed-header.bin` contains the header after Kbit/Kc/ICVPS2 have been placed at the same offsets as PS2SDK `libsecr`.

The F2 trace uses a success bitmap where `0x0f` means the stock `F2/50`, `F2/51`, `F2/52` and `F2/53` sequence completed for that 8-byte half.

## Validated ICV-enabled KELF

Candidate A is the first hardware-validated experiment vector:

```text
flags       = 0x022e
header size = 0x0088
SHA-256     = e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a
```

The next vector, Candidate C, preserves the validated format and payload but deliberately uses four distinct plaintext Kbit/Kc halves. It is intended to produce four independent session/F2 equations per transaction instead of repeating the same plaintext block four times.

Candidate C SHA-256:

```text
87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d
```

The reproducible research generator is:

```text
tools/make_icv_test_kelf.py
```

It intentionally contains no PlayStation key material and requires a user-supplied kelftool-compatible `PS2KEYS.dat`.

## UI / safety

The frontend is a deliberately small derivative of the interaction language used by `fhdb-bootstrap-manager`: dark status panels, highlighted two-line menu cards, short contextual hints and `X` to open/continue.

The probe does not intentionally write MechaCon NVRAM, console IDs, EEPROM data or memory-card filesystem contents. It does execute real SECR/MagicGate transactions against the selected card and MechaCon.

A negative dev.9 experiment showed that forcing MCMAN's auth-reset path can leave a confusing warm-boot security state. The current Browser-return path resets the IOP back to the ROM environment before `ExecOSD`; controlled research runs should still use a full power-off between samples.

Do not treat a warm-return FMCB detection failure as evidence of filesystem corruption without first preserving evidence and performing a real cold-power check.

## Build

With a current PS2DEV / PS2SDK environment:

```sh
make
```

For a stripped hardware build:

```sh
make release
```

The repository CI uses `ps2dev/ps2dev:v2.0.0`. The instrumented security backend is built from the pinned PS2SDK security commit recorded by the experiment documentation.

## PS2SDK basis

The implementation is based on public PS2SDK `libsecr` / `secrman` behavior and embeds the required PS2SDK IOP modules in the ELF so results do not depend on the launcher's module set.

Timing or subtle state claims are not considered final until reproduced on real hardware. PCSX2 remains useful for correctness and inspection, not as the sole authority for MechaCon/MagicGate state behavior.
