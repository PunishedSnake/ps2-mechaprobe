# ICVPS2 signed-KELF experiment

This note records the controlled reconstruction and hardware validation of a KELF that genuinely declares `Uses_ICVPS2`, rather than changing the flag after signing.

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

## Hardware validation result

Candidate A is accepted by retail hardware. Multiple cold-boot runs completed the full native path:

`SecrDownloadHeader -> encrypted block -> SecrDownloadGetKbit -> SecrDownloadGetKc -> SecrDownloadGetICVPS2 -> complete`

The returned BIT reports header size `0x88`, matching the reconstructed layout. `SecrDownloadGetICVPS2()` succeeds and returns eight bytes which are stored in the explicit `0x80..0x87` slot.

Nine controlled runs were completed with three MagicGate-capable cards: two Sony 8 MB cards and one unbranded 64 MB card. All nine runs completed successfully with the exact same Candidate A input.

Observed behavior:

- final Kbit/Kc are stable across repeated cold boots with the same physical card;
- final Kbit/Kc differ between the tested physical cards;
- ICVPS2 changes between transactions even when the physical card and KELF bytes remain unchanged;
- all nine captured ICVPS2 values were distinct.

The current evidence therefore rules out ICVPS2 being merely a fixed KELF hash or fixed card identifier. It is consistent with additional transaction/session state inside the MechaCon path. More instrumentation is required before attributing that changing state to a specific nonce/challenge.

Exact per-card cryptographic outputs are intentionally kept in the local evidence bundles rather than committed to the public repository.

## Candidate B: root-slot reuse

A fallback hypothesis keeps the header size at `0x0080`, recalculates the structures for flags `0x022e`, omits the normal root-signature value from the final slot and leaves `0x78..0x7f` zero for ICVPS2.

Candidate B SHA-256:

`b7f2d4af14c10ce75a499172fe04d587b73a6157330f2e2630fdb3d22702381d`

Candidate B is no longer required for the primary format hypothesis because Candidate A has been accepted by hardware. Keep it only as historical evidence of the alternative layout considered before validation.

## Port-dependence result

Dev.6 added `Native control - mc1` and a scrolling menu so the same physical card could be tested through both memory-card ports without changing the transaction mode.

The already-characterized Sony 8 MB card from the mc0 series was moved to mc1 and tested after a cold boot with byte-identical Candidate A.

The mc1 run completed successfully. Its final Kbit and Kc are byte-for-byte identical to all three earlier mc0 runs made with the same physical card. The returned BIT is also byte-for-byte identical. Comparing the 136-byte processed headers shows that the only differences between the mc1 result and each mc0 result are offsets `0x80..0x87`, i.e. the eight-byte ICVPS2 slot.

The mc1 ICVPS2 value was again different from all prior captures, consistent with the already-observed per-transaction variability.

This result rules out the physical mc0/mc1 port as a determinant of the final card-wrapped Kbit/Kc for this card and KELF. The logical-to-physical SIO2 bridge remains required for communication, but once the correct physical channel is selected the resulting wrapped key material is card-dependent rather than port-dependent.

No MechaCon NVRAM/EEPROM writes are part of this protocol.

## Historical note

Older FreeMcBoot code already contains a direct SCMD `0x98` ICVPS2 read path and describes the result as eight bytes, but its source comments state that no known encrypted file used the field at the time. Current PS2SDK preserves the same operation. The present experiment therefore does not claim discovery of the command itself; its contribution is a reproducible ICV-enabled KELF layout and controlled retail-hardware captures.
