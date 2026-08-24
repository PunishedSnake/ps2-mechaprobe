# ICVPS2 signed-KELF experiment

This note records the first controlled attempt to produce an input KELF that genuinely declares `Uses_ICVPS2`, rather than changing the flag after signing.

## Hardware baseline

The dev.5 probe completed a native FreeMcBoot 1.966 `FMCB.XLF` download transaction on real PS2 hardware after translating logical memory-card ports 0/1 to the physical SIO2 channels 2/3 required by SECRMAN CardAuth.

The same untouched KELF completed Header -> encrypted block -> Kbit -> Kc across separate cold boots. A deliberately late unconditional `SecrDownloadGetICVPS2()` then failed, showing that a normal `0x022c` KELF does not establish the MechaCon state required by SCMD `0x98`.

## Source template

The reference input is FreeMcBoot 1.966 `FMCB.XLF`:

- size: 78736 bytes
- SHA-256: `edd1931816ca22d96835cea17ff14846b3fecb5a6ea9b346a3b079c4394508d9`
- flags: `0x022c`
- KELF header size: `0x0080`
- application type: `1`
- MagicGate zones: `0xff`

The public KELF crypto material was validated against this exact file before generating any experiment: the calculated header signature, BIT signature, root signature and signed/encrypted block signature all match the source. Reconstructing the ordinary `0x022c` file from the parsed/decrypted structures reproduces the source byte-for-byte.

## Candidate A: extended ICV slot

Candidate A keeps the same user header, payload, plaintext Kbit/Kc and two-block content layout while changing only the structure required by the ICV hypothesis:

- flags: `0x022e`
- header size: `0x0088`
- returned-BIT header-size field prepared as `0x0088`
- header signature recalculated
- BIT signature recalculated
- root signature recalculated
- an additional zero-filled 8-byte field is appended after the root signature
- payload moves from offset `0x80` to `0x88`

The final eight bytes of the header are therefore an explicit ICVPS2 placeholder at `0x80..0x87`, matching PS2SDK `libsecr` behavior which stores the value at `KELF_header_size - 8`.

Candidate A SHA-256:

`e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a`

This is the preferred first hardware test. It is still an experimental reconstruction until MechaCon accepts the header and returns ICVPS2.

## Candidate B: root-slot reuse

A fallback hypothesis keeps the header size at `0x0080`, recalculates the structures for flags `0x022e`, omits the normal root-signature value from the final slot and leaves `0x78..0x7f` zero for ICVPS2.

Candidate B SHA-256:

`b7f2d4af14c10ce75a499172fe04d587b73a6157330f2e2630fdb3d22702381d`

Do not mix Candidate A and Candidate B in the same baseline session. Candidate B is only useful if Candidate A is rejected and its failure evidence has first been inspected.

## First test protocol

Use the already hardware-validated dev.5 probe without changing its code. Put Candidate A at `mass:/PS2DF-MECHA/input.kelf`, cold boot, use the same known-good MagicGate card as the successful native control, and select `Native control - mc0`.

Because the input itself has `Uses_ICVPS2 = 1`, native-control mode follows the normal staged path and performs exactly one ICV read after successful Header -> Blocks -> Kbit -> Kc. Do not use the legacy unconditional-ICV mode for this experiment.

Possible results are intentionally distinguishable:

- `SecrDownloadHeader` failure: the reconstructed ICV-enabled header/layout is not accepted.
- returned BIT header size differs from `0x88`: important evidence about the authentic layout.
- Kbit/Kc succeeds but ICV fails: the flag/layout is accepted but another semantic field is required.
- complete + `icvps2.bin`: first successful ICVPS2 capture.

## Card-dependence protocol after first success

Keep the exact same Candidate A bytes for every comparison. Cold boot between every run.

1. Known-good Card A, three runs.
2. Card B, two or three runs.
3. Card C, two or three runs.
4. Optionally repeat one card in physical port 2 using the mc1 path.

Record which `RUNxxxx` belongs to each physical card. Compare processed Kbit, Kc and ICVPS2 separately. This can distinguish card-dependent key wrapping from the actual ICVPS2 dependency.

No MechaCon NVRAM/EEPROM writes are part of this protocol.
