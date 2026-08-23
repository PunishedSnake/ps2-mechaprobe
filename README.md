# PS2 Mecha Probe

Developer-oriented PlayStation 2 homebrew for probing the MechaCon / MagicGate KELF download path and collecting reproducible ICVPS2 evidence.

The first target is not ICVPS2 emulation. It is answering a more basic question on real hardware: **what does MechaCon return for SCMD `0x98`, and what inputs or session state make that value change?**

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

## Input

Create this directory on a FAT USB device:

```text
mass:/PS2DF-MECHA/
```

Put the KELF to test at:

```text
mass:/PS2DF-MECHA/input.kelf
```

The current development build accepts KELFs up to 16 MiB. The selected `mc0` or `mc1` must contain a compatible PS2 MagicGate memory card because the download-header and key stages use the card authentication path.

## Evidence bundle

Each run creates the next free directory:

```text
mass:/PS2DF-MECHA/RUN0001/
mass:/PS2DF-MECHA/RUN0002/
...
```

A successful ICVPS2 run contains:

```text
probe.json
input.kelf
input.sha256
processed-header.bin
bit-table.bin
icvps2.bin
icvps2.txt
```

`probe.json` records the input identity, KELF header flags, BIT counts, transaction progress, ROMVER, best-effort model information, raw `sceCdMV()` data, MagicGate region/version interpretation, RTC value and ICVPS2.

`processed-header.bin` is especially useful for reverse engineering because it contains the header after the Kbit/Kc/ICVPS2 material has been placed at the same offsets as PS2SDK `libsecr`.

## First hardware experiment

For a KELF that has `Uses_ICVPS2 = true`:

```text
KELF A -> run 1 -> reboot
KELF A -> run 2 -> reboot
KELF A -> run 3 -> reboot
KELF B -> run 4 -> reboot
KELF C -> run 5
```

Compare the SHA-256 and ICVPS2 values. This distinguishes the most useful hypotheses:

- stable per console,
- stable per KELF,
- dependent on both console and KELF,
- dependent on session state as well.

Do not mix an additional raw `0x98` call into these baseline runs. A later probe mode can intentionally test the post-download command state as a separate experiment.

## UI

The frontend is a deliberately small derivative of the interaction language used by `fhdb-bootstrap-manager`: dark status panels, highlighted two-line menu cards, short contextual hints, `X` to open/continue and `TRIANGLE` to go back. It keeps the hardware-proven `libdebug` GS bootstrap while avoiding the HDD manager's unrelated video-mode, theme and APA dependencies.

## Build

With a current PS2DEV / PS2SDK environment:

```sh
make
```

For a stripped hardware build:

```sh
make release
```

The repository CI uses `ps2dev/ps2dev:v2.0.0` and uploads `PS2_MECHAPROBE.ELF` plus its SHA-256 checksum.

## Safety / current limitations

This is a developer probe. It does not write MechaCon NVRAM, console IDs, EEPROM data or memory-card filesystem contents itself. It does perform the normal SECR/MagicGate KELF download transaction against the selected card and MechaCon.

Current limitations:

- one fixed USB input path,
- memory-card slot 0 only for each physical port,
- no KELF browser yet,
- no automated multi-run comparison yet,
- no standalone/raw `0x98` experiment in the baseline mode,
- model-name queries may be unavailable with the retail ROM CDVDMAN and are therefore best-effort evidence only.

## PS2SDK basis

The implementation is based on the public PS2SDK `libsecr` / `secrman` behavior and embeds the required PS2SDK IOP modules in the ELF so results do not depend on the launcher's module set.
