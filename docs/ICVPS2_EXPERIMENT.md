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

## dev.7 / dev.10 session-wrapped key result

One-pass instrumentation around `scePreEncryptKbit`, `scePreEncryptKc` and the stock `card_encrypt()` path establishes where the cold-boot variability appears.

Across repeated runs with a byte-identical Candidate A and the same Sony 8 MB card:

- the raw values returned by MechaCon `0x94/0x95` and `0x96/0x97` change between cold boots;
- the final card-wrapped Kbit/Kc remain stable for the physical card;
- the normal `0x98` ICVPS2 value also changes between transactions;
- dev.10 records all four stock `F2/50 -> F2/51 -> F2/52 -> F2/53` transforms as successful (`mask=0x0f`), without replaying or adding any card command;
- the three dev.10 runs in this dataset used logical `mc1`, producing physical SECR/SIO2 port 3, and retained the same final card-wrapped key material previously observed for this card.

The processed 136-byte headers from the dev.10 series differ from one another only at offsets `0x80..0x87`, the ICVPS2 slot. Kbit/Kc in the processed header are otherwise identical across the runs.

Public `ps3mca_tool` source provides the matching algorithmic model: after the disk KELF content key is decrypted, each 8-byte Kbit/Kc half is encrypted with single DES under the current MagicGate session key before being sent for card-side binding. Candidate A decrypts to four identical 8-byte plaintext content-key blocks, which explains why all four `0x94..0x97` pre-key values within a given run are identical. The observed cold-boot variation is therefore consistent with a changing session key, while the card-side F2 binding converts those session-wrapped inputs into stable card-bound output for the same physical card.

This is stronger than merely observing changing ICVPS2: it locates session-dependent variability before the card-side F2 transform. The next controlled vector should use deliberately distinct plaintext Kbit/Kc halves so that one transaction yields four independent input/output equations under one session instead of repeating the same block four times.

## dev.8 / dev.9 negative-state experiments

Attempts to force or explicitly trigger a fresh full memory-card authentication before the KELF transaction changed the state being measured and were therefore abandoned.

Dev.8 inserted a second `SecrAuthCard()` immediately before `SecrDownloadHeader`; the transaction then failed before normal KELF processing. Dev.9 instead triggered `mcGetInfo/mcSync`, but current MCMAN returned `sceMcResFailResetAuth (-11)` from its auth-reset path before the KELF transaction. Current PS2SDK implements that reset using the card-side `F3` command.

A warm return to Browser after the dev.9 failure also caused an FMCB-bearing card to remain filesystem-visible while FMCB was not detected. At that point the probe's custom IOP security stack was still resident because the old Browser-return path called `ExecOSD` without restoring the ROM IOP environment. Dev.10 fixes this confounder by resetting the IOP before `ExecOSD`. No claim of persistent card-data modification is made from the warm-boot observation.

## Historical note

Older FreeMcBoot code already contains a direct SCMD `0x98` ICVPS2 read path and describes the result as eight bytes, but its source comments state that no known encrypted file used the field at the time. Current PS2SDK preserves the same operation. The present experiment therefore does not claim discovery of the command itself; its contribution is a reproducible ICV-enabled KELF layout and controlled retail-hardware captures.
