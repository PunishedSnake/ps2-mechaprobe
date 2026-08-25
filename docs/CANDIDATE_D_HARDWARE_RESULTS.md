# Candidate D real-hardware results

Candidate D was designed to distinguish a stateless card-side F2 mapping from hidden position/state dependence by repeating the same two plaintext content-key blocks in both Kbit and Kc.

Evidence labels follow the project convention: **CONFIRMED**, **CURRENT IMPLEMENTATION**, **INFERENCE**, and **TEST HYPOTHESIS**.

## Supplied evidence

```text
archive: 3cardtest.zip
size:    244387 bytes
SHA-256: ff631ec44bb62b6759310b6e35a3c70ae1059e8bfea88ed078613d6471ae0801
```

The archive contains three complete successful RUN directories, one from each previously characterized card.

Candidate D:

```text
A = 0123456789abcdef
B = fedcba9876543210

Kbit = A || B
Kc   = A || B

SHA-256     = 437ab24cf29476d372071af60b9de0d86ae15d8f67fc0ee4061c19fa46b50670
size        = 78744 bytes
flags       = 0x022e
header size = 0x0088
```

Offline construction metadata:

```text
header signature         = 84c3ebbfcf59eb2b
BIT signature            = 22a235f5a3ad7123
root signature           = 6e45eb67f7446ad0
block0 signature         = 517193d4c6b693ea
plaintext payload SHA256 = d7922c6eb6cce23c05808a77ffe0c2c58758414d707da4aebd85a7fabec9d69d
```

The probe is the unchanged hardware-successful `0.1.0-dev.10` F2 trace build.

**CONFIRMED:** all three `input.kelf` files are byte-identical Candidate D.

**CONFIRMED:** all three runs returned the same BIT:

```text
600486e0f71109358b650c05adbfa974bd71bf00055291b7475628b68265d89e
```

**CONFIRMED:** all three transactions completed Header -> encrypted block -> Kbit -> Kc -> ICVPS2 with status `success`, code `0`.

**CONFIRMED:** every one of the four F2 records in every run completed `F2/50`, `F2/51`, `F2/52`, `F2/53` with `mask=0x0f`, `failed=0x00`.

---

# Card identity recovery

RUN names are not used as card identity. Candidate C already established the stable result for:

```text
B = fedcba9876543210
```

on both Sony cards:

```text
SONY-1: 111b7019b53114c3
SONY-2: b559bdac76c898b0
```

Candidate D repeats the same plaintext, so the mapping is independently recoverable from the evidence:

```text
RUN0001 = 8MB-MG-SONY-2
RUN0002 = 8MB-MG-SONY-1
RUN0003 = 64MB-MG-NOTSONY
```

Port assignment:

```text
RUN0001 / SONY-2   logical mc0 -> physical SECR/SIO2 2
RUN0002 / SONY-1   logical mc1 -> physical SECR/SIO2 3
RUN0003 / NONSONY  logical mc1 -> physical SECR/SIO2 3
```

The decisive Candidate-D comparisons are within individual transactions, so the mixed port assignment is not a confounder for the position/state result. Earlier hardware testing also established that correct mc0/mc1 routing does not change the tested Sony card-bound Kbit/Kc values.

---

# RUN0001 - 8MB-MG-SONY-2

```text
RTC:   2026-08-26T02:20:48
ICV:   6b156fd975d01290

pre Kbit = 716338e2428c50b84dd40f4c6b78843f
pre Kc   = 716338e2428c50b84dd40f4c6b78843f

A / Kbit0: 716338e2428c50b8 -> 890b39b28786304c
B / Kbit1: 4dd40f4c6b78843f -> b559bdac76c898b0
A / Kc0:   716338e2428c50b8 -> 890b39b28786304c
B / Kc1:   4dd40f4c6b78843f -> b559bdac76c898b0

final Kbit = 890b39b28786304cb559bdac76c898b0
final Kc   = 890b39b28786304cb559bdac76c898b0

processed-header SHA-256 =
a1b0a9813f55b1492fa3223ffc387514d24fa54e25cdbd2fe332fc49adfe4051
```

**CONFIRMED:** repeated A and B have identical pre-card inputs and identical F2 outputs across Kbit/Kc positions.

**CONFIRMED cross-vector reproduction:** `B -> b559bdac76c898b0` exactly reproduces the SONY-2 Candidate-C result although Candidate C placed B at Kc0, Candidate D places it at Kbit1/Kc1, and the MagicGate session differs.

---

# RUN0002 - 8MB-MG-SONY-1

```text
RTC:   2026-08-26T02:21:38
ICV:   686b1683e8f2f73c

pre Kbit = 3da99d5782e7be586fcaaad36d22521a
pre Kc   = 3da99d5782e7be586fcaaad36d22521a

A / Kbit0: 3da99d5782e7be58 -> 1b0d96b2404d2a10
B / Kbit1: 6fcaaad36d22521a -> 111b7019b53114c3
A / Kc0:   3da99d5782e7be58 -> 1b0d96b2404d2a10
B / Kc1:   6fcaaad36d22521a -> 111b7019b53114c3

final Kbit = 1b0d96b2404d2a10111b7019b53114c3
final Kc   = 1b0d96b2404d2a10111b7019b53114c3

processed-header SHA-256 =
e8a16f02547854a9b4ddb032bb7b9fda7d0944b67fc2dead7f51492ab9d9c7fa
```

**CONFIRMED:** repeated A and B have identical pre-card inputs and identical F2 outputs across Kbit/Kc positions.

**CONFIRMED cross-vector reproduction:** `B -> 111b7019b53114c3` exactly reproduces the SONY-1 Candidate-C result across a different session and KELF position.

---

# RUN0003 - 64MB-MG-NOTSONY

```text
RTC:   2026-08-26T02:22:47
ICV:   dee682e3e7d2806e

pre Kbit = 3cfefcaf0eef59cbf1f36053fdd6910c
pre Kc   = 3cfefcaf0eef59cbf1f36053fdd6910c
```

Candidate D produces byte-identical repeated F2 inputs:

```text
A / Kbit0 input = 3cfefcaf0eef59cb
A / Kc0   input = 3cfefcaf0eef59cb

B / Kbit1 input = f1f36053fdd6910c
B / Kc1   input = f1f36053fdd6910c
```

The card nevertheless returns different outputs:

```text
A / Kbit0: 3cfefcaf0eef59cb -> 14e38596a80cbac3
A / Kc0:   3cfefcaf0eef59cb -> 52ce42d5a1b1570a

B / Kbit1: f1f36053fdd6910c -> 602b62fd8b6edf57
B / Kc1:   f1f36053fdd6910c -> 6c844a79b9127a96

final Kbit = 14e38596a80cbac3602b62fd8b6edf57
final Kc   = 52ce42d5a1b1570a6c844a79b9127a96

processed-header SHA-256 =
31e3dc6c809bc585c19a414057cc93f1c04380f21114d650fe3bc59e56f3d84f
```

## Decisive result

**CONFIRMED:** on the tested NONSONY card, final F2 output is not a stateless function only of `(physical card, plaintext content key)`.

Candidate D controls the F2 boundary directly:

```text
same physical card
same transaction
same active session
same port
same plaintext
same 8-byte pre-card/F2 input
same visible F2/50 -> 51 -> 52 -> 53 sequence
but
Kbit-position output != Kc-position output
```

The divergence is therefore introduced at or behind the card-side F2 boundary after the byte-identical input has been supplied.

**CONFIRMED:** hidden card-side context/state/history participates in the NONSONY transform.

**NOT YET DISTINGUISHED:** these data do not uniquely separate:

- fixed position-specific transform/key/IV behavior;
- state carried between F2 operations;
- context established by preceding MagicGate operations;
- another deterministic implementation-specific mechanism.

The visible F2 opcodes are the same for every 8-byte half, so no explicit Kbit/Kc selector is transmitted in the traced `50/51/52/53` sequence.

---

# Relationship to Candidate A and Candidate C

Candidate A used the same plaintext four times:

```text
3939393939393939
```

Both Sony cards returned one repeated result. NONSONY returned four different stable results:

```text
Kbit0 -> 160a03515708a1e4
Kbit1 -> 24f64cdcc57f6c02
Kc0   -> 14d9895d4a9fd983
Kc1   -> 3b81b6bd30f92c74
```

Candidate C then established stable per-position NONSONY results across cold boots with four distinct plaintexts.

Candidate D closes the key ambiguity left by those datasets:

**CONFIRMED:** NONSONY divergence is not caused by different session-wrapped inputs at the repeated positions. Candidate D makes those F2 inputs byte-identical and the outputs still diverge.

For both Sony cards, Candidate D strengthens the opposite result:

**CONFIRMED for `B = fedcba9876543210`:** the same plaintext reproduces the same card-bound value across Candidate C and Candidate D, despite different session state and KELF position.

---

# ICVPS2

Three additional values were captured:

```text
SONY-2  6b156fd975d01290
SONY-1  686b1683e8f2f73c
NONSONY dee682e3e7d2806e
```

None duplicates a previously recorded Candidate-A/C value.

**CONFIRMED:** the Candidate-D data remain consistent with ICVPS2 being transaction-variable rather than a fixed KELF, card, or `(KELF, card)` value. One Candidate-D run per card does not by itself add a new repeated-sample variability claim; that remains established by the earlier cold-boot series.

---

# Binary comparison

All three `processed-header.bin` files are 136 bytes. Pairwise semantic differences remain confined to:

```text
0x28..0x47  final card-bound Kbit/Kc
0x80..0x87  ICVPS2
```

The exact differing-byte count between the Sony headers is 38 because two bytes happen to match, but no additional structural range differs.

---

# Current model after Candidate D

## CONFIRMED - Sony cards

For the tested genuine Sony cards:

```text
plaintext content-key half
  -> transaction-variable MechaCon/session representation
  -> card F2/50-53
  -> stable card-specific representation
```

The repeated B vector survives changes of session and KELF position exactly. This is consistent with public `ps3mca_tool` descriptions of removing SessionKey wrapping and returning a storage/card-bound representation.

## CONFIRMED - NONSONY

The unbranded 64 MB card is behaviorally different. Its final F2 output depends on hidden internal state/context in addition to the supplied pre-card input.

## INFERENCE

A useful descriptive model is therefore:

```text
F2_output = G(card_internal_state_or_context, pre_card_input)
```

This notation does not claim a specific internal cipher or key derivation.

---

# Next discrimination work

Candidate D proves hidden context/state participation but does not identify its exact form.

The highest-information next KELF is a complementary `B/A/B/A` vector. Together with Candidate D and Candidate C it can place A and B across all four Kbit/Kc positions and build a small plaintext-by-position matrix for NONSONY instead of collecting more unrelated random vectors.

This is a **TEST HYPOTHESIS**.

The independent ICVPS2/SessionKey path remains dev.11 passive-auth capture. Do not combine a new KELF-position experiment and new auth instrumentation into the same primary dataset.
