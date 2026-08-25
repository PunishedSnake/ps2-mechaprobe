# PS2 Mecha Probe

Developer-oriented PlayStation 2 homebrew for probing the MechaCon / MagicGate KELF download path and collecting reproducible ICVPS2 evidence.

The project started with a basic question: **what does MechaCon return for SCMD `0x98`, and what inputs or session state make that value change?** It has progressed into direct real-hardware observation of session-wrapped Kbit/Kc, stock card-side F2 binding, card-implementation differences and transaction-variable ICVPS2.

## Current hardware-validated status

The following are confirmed on real retail PS2 hardware:

- logical memory-card ports used by EE/libsecr must be translated to the physical SIO2 channels expected by the SECRMAN card path (`mc0 -> 2`, `mc1 -> 3`);
- a cryptographically reconstructed KELF with `Uses_ICVPS2=1`, flags `0x022e`, header size `0x88` and a dedicated 8-byte ICV slot is accepted by retail MechaCon;
- the normal native path completes `Header -> encrypted block -> Kbit -> Kc -> ICVPS2` and returns a real 8-byte SCMD `0x98` value;
- ICVPS2 changes between transactions even with byte-identical KELF and the same physical card;
- final card-wrapped Kbit/Kc are stable per tested physical card but differ between cards;
- MechaCon pre-card material returned by `0x94..0x97` changes between cold boots;
- dev.10 directly traces all four stock `F2/50 -> F2/51 -> F2/52 -> F2/53` operations without replay;
- Candidate C, with four distinct plaintext content-key halves, completed 9/9 real-hardware runs across two Sony 8 MB cards and one unbranded 64 MB MagicGate-capable card;
- in all nine Candidate-C runs the pre-card values changed with the transaction while each corresponding final F2 output stayed stable for a fixed physical card/position;
- Candidate D repeats plaintexts as `A/B/A/B` and completed one successful run on each of the same three cards;
- on both Sony cards Candidate D gives byte-identical F2 outputs when a plaintext is repeated in Kbit and Kc, and plaintext `fedcba9876543210` reproduces the exact Candidate-C card-bound output across a different session and different KELF position;
- on the unbranded 64 MB card Candidate D supplies byte-identical repeated F2 inputs inside one transaction but receives different outputs at the Kbit and Kc positions, directly proving hidden card-side context/state dependence behind the F2 boundary;
- dev.11 passively watched for a naturally successful full `SecrAuthCard()` after the probe's own IOP reset/module load; the first real-hardware Candidate-C run completed normally with `auth_trace_present=false`, proving that no newly observed full `SecrAuthCard()` is required inside that post-reset instrumented IOP window for the successful `0x94..0x98` + F2 transaction;
- the dev.11 result places the active session/auth state earlier than the observed post-reset SECRMAN window, although the exact earlier creator and storage location remain unproven;
- mc0/mc1 does not change final Sony card-bound Kbit/Kc once the correct physical SIO2 channel is selected;
- injecting a second full `SecrAuthCard` or forcing MCMAN's `F3` reset-auth path changes the security state and is not part of the controlled KELF-binding experiment.

Research records:

- [`docs/ICVPS2_RESEARCH_RECORD.md`](docs/ICVPS2_RESEARCH_RECORD.md) - original full experiment timeline through Candidate C preparation;
- [`docs/CANDIDATE_C_HARDWARE_RESULTS.md`](docs/CANDIDATE_C_HARDWARE_RESULTS.md) - exact nine-run Candidate C dataset;
- [`docs/CANDIDATE_D_HARDWARE_RESULTS.md`](docs/CANDIDATE_D_HARDWARE_RESULTS.md) - exact three-card A/B/A/B result and direct NONSONY state/context proof;
- [`evidence/candidate-d/manifest.json`](evidence/candidate-d/manifest.json) - machine-readable Candidate D run mapping, hashes, ports and F2 vectors;
- [`docs/DEV11_PASSIVE_AUTH_RESULTS.md`](docs/DEV11_PASSIVE_AUTH_RESULTS.md) - first passive-auth hardware result and session-lifetime inference;
- [`evidence/dev11-run0002/manifest.json`](evidence/dev11-run0002/manifest.json) - machine-readable dev.11 Candidate-C/F2/ICV evidence;
- [`docs/ICVPS2_EXPERIMENT.md`](docs/ICVPS2_EXPERIMENT.md) - ICV-enabled KELF reconstruction;
- [`docs/SECR_TRACE_DEV7.md`](docs/SECR_TRACE_DEV7.md) - first one-pass pre-card trace.

## Transaction model currently supported by hardware evidence

Public `ps3mca_tool` source models each plaintext Kbit/Kc half as encrypted under the active MagicGate SessionKey before card binding. Its F2 implementation describes the card as removing session-key wrapping and re-encrypting the content key with card storage-key material.

For the two tested Sony cards, the probe's real-hardware observations are consistent with that boundary:

```text
plaintext Kbit/Kc half
        |
        v
MechaCon/session transform (0x94..0x97)
        |
        v
transaction-variable pre-card value
        |
        v
stock F2/50 -> 51 -> 52 -> 53
        |
        v
stable card-bound value for the physical card/plaintext
```

Candidate D demonstrates that the third-party 64 MB card cannot be described by the same simple external mapping alone. With an identical F2 input and identical visible F2 command sequence, it returns different results at different points in the transaction. A descriptive model therefore needs hidden card-side state/context:

```text
F2_output = G(card_internal_state_or_context, pre_card_input)
```

This does not yet distinguish a fixed position-specific transform from sequential state/history inside the third-party card.

Dev.11 adds a lifetime constraint to the session model. A complete Candidate-C transaction succeeded after the probe reset the IOP and loaded the instrumented SECRMAN, but that SECRMAN observed no successful full `SecrAuthCard()` before `0x94..0x98` were used. The simplest current explanation is that the relevant session/auth state predates this instrumented window and survives the IOP reset outside ordinary IOP RAM. Which earlier boot/auth step created that state remains a test hypothesis rather than a confirmed claim.

ICVPS2 is returned in parallel from normal SCMD `0x98` and remains transaction-variable in the collected datasets. Its exact dependency is not yet reconstructed.

## Probe scope

The probe builds a known IOP environment with PS2SDK modules, initializes `SECRMAN` / `SECRSIF`, processes one KELF through an instrumented equivalent of `SecrDownloadFile()`, and exports evidence to USB.

The public sequence is executed stage by stage:

1. `SecrDownloadHeader(port, slot, ...)`
2. `SecrDownloadBlock(...)` for encrypted BIT entries
3. `SecrDownloadGetKbit(port, slot, ...)`
4. `SecrDownloadGetKc(port, slot, ...)`
5. `SecrDownloadGetICVPS2(...)` exactly once when the KELF requests ICVPS2
6. store Kbit, Kc and ICVPS2 at the normal PS2SDK offsets.

This avoids confusing a second post-transaction `0x98` with the value belonging to the actual KELF transaction.

## Input

Use a FAT USB device with:

```text
mass:/PS2DF-MECHA/input.kelf
```

Current development builds accept KELFs up to 16 MiB. Slot 0 of the selected physical memory-card port is used by the experiment.

## Evidence bundle

Each run creates the next free directory under:

```text
mass:/PS2DF-MECHA/RUNxxxx/
```

A successful F2-trace run may include:

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

Dev.11 additionally passively reports whether the instrumented SECRMAN observed a naturally occurring successful full `SecrAuthCard()` after module load. It never starts or resets that auth itself. The decisive status is written to:

```text
auth-trace-status.txt
```

The first real-hardware dev.11 run returned `auth_trace_present=false` while the complete Candidate-C transaction still succeeded. Therefore absence of an auth transcript is a valid and informative result rather than a probe failure.

If a future run observes one, the full transcript is also exported as `auth-trace.bin` plus CardIV/CardMaterial/CardNonce, MechaChallenge1..3 and CardResponse1..3 files.

## Validated / controlled KELF vectors

### Candidate A

First hardware-validated ICV-enabled format:

```text
flags       = 0x022e
header size = 0x0088
SHA-256     = e317a0a287939a93961a376f0bd3ed47e051029ee2e45f1fc17ca8db5f22d27a
```

Its four plaintext content-key halves are identical (`3939393939393939`).

### Candidate C

Four independent plaintext blocks in one session:

```text
Kbit0 = 0011223344556677
Kbit1 = 8899aabbccddeeff
Kc0   = fedcba9876543210
Kc1   = 0f1e2d3c4b5a6978
SHA-256 = 87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d
```

Candidate C is hardware-validated across all three test cards and was also used for the first dev.11 passive-auth test.

### Candidate D

A/B/A/B position/state discrimination vector:

```text
A = 0123456789abcdef
B = fedcba9876543210
Kbit = A || B
Kc   = A || B
SHA-256 = 437ab24cf29476d372071af60b9de0d86ae15d8f67fc0ee4061c19fa46b50670
```

Candidate D is hardware-validated with one successful run on each test card. It confirms simple repeated-plaintext behavior for both Sony cards and directly proves hidden F2 state/context dependence on the unbranded 64 MB card.

The reproducible vector generator is:

```text
tools/make_icv_test_kelf.py
```

It intentionally contains no PlayStation key material and requires a user-supplied kelftool-compatible `PS2KEYS.dat`.

## Safety

The probe does not intentionally write MechaCon NVRAM, console IDs, EEPROM data or memory-card filesystem contents. It does execute real SECR/MagicGate transactions against the selected card and MechaCon.

A negative dev.9 experiment showed that forcing MCMAN's auth-reset path can leave a confusing warm-boot security state. Current Browser return resets the IOP to the ROM environment before `ExecOSD`; controlled research runs should still use a full power-off between samples.

Do not treat warm-return FMCB detection failure as filesystem corruption without preserving evidence and performing a real cold-power check first.

## Build

```sh
make
```

or stripped:

```sh
make release
```

CI uses `ps2dev/ps2dev:v2.0.0` and an explicitly pinned PS2SDK security revision for the instrumented backend.

PCSX2 is useful for correctness/inspection, but state claims in this project are considered final only after real-hardware reproduction.
