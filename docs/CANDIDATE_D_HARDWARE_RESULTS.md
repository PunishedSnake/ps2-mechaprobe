# Candidate D real-hardware results

This document records the first real-hardware Candidate D dataset. Candidate D was designed specifically to distinguish a stateless card-side mapping from hidden F2 position/state dependence by repeating the same two plaintext content-key blocks in both Kbit and Kc.

## Evidence class

The terminology follows the project research convention:

- **CONFIRMED** - byte-level evidence or reproduction on real PlayStation 2 hardware;
- **CURRENT IMPLEMENTATION** - behavior of the pinned PS2SDK/security stack used by the probe;
- **INFERENCE** - an explanation consistent with the evidence but not uniquely proven;
- **TEST HYPOTHESIS** - a controlled experiment proposed to distinguish remaining models.

## Supplied archive

Operator-supplied archive:

```text
3cardtest.zip
size    = 244387 bytes
SHA-256 = ff631ec44bb62b6759310b6e35a3c70ae1059e8bfea88ed078613d6471ae0801
```

It contains three complete successful RUN directories, one from each previously characterized physical MagicGate-capable card.

The RUN directory names are not treated as card identity. Card identity is recovered from the already-established Candidate-C card-bound vectors, as described below.

---

# 1. Candidate D input

Candidate D deliberately repeats two plaintext blocks:

```text
A = 0123456789abcdef
B = fedcba9876543210

Kbit half0 = A
Kbit half1 = B
Kc   half0 = A
Kc   half1 = B
```

Combined:

```text
Kbit = 0123456789abcdeffedcba9876543210
Kc   = 0123456789abcdeffedcba9876543210
```

KELF properties:

```text
SHA-256             = 437ab24cf29476d372071af60b9de0d86ae15d8f67fc0ee4061c19fa46b50670
size                 = 78744 bytes
flags                = 0x022e
header size          = 0x0088
header signature     = 84c3ebbfcf59eb2b
BIT signature        = 22a235f5a3ad7123
root signature       = 6e45eb67f7446ad0
block0 signature     = 517193d4c6b693ea
plaintext payload SHA-256 = d7922c6eb6cce23c05808a77ffe0c2c58758414d707da4aebd85a7fabec9d69d
```

The probe binary is the unchanged hardware-successful dev.10 F2 trace build.

**CONFIRMED:** all three supplied `input.kelf` files are byte-identical Candidate D with the SHA-256 above.

**CONFIRMED:** all three runs returned the same BIT:

```text
SHA-256 = 600486e0f71109358b650c05adbfa974bd71bf00055291b7475628b68265d89e
```

**CONFIRMED:** all three transactions completed Header -> encrypted block -> Kbit -> Kc -> ICVPS2 with status `success`, code `0`.

**CONFIRMED:** all eight traced F2 records per card pair set? More precisely, each run contains four F2 records, and every record completed `F2/50`, `F2/51`, `F2/52`, `F2/53` with `mask=0x0f` and `failed=0x00`.

---

# 2. Card identity recovery

The archive itself contains RUN numbers rather than physical-card labels. Identity can nevertheless be recovered without relying on run order.

Candidate C previously established the stable output for plaintext:

```text
B = fedcba9876543210
```

on both Sony cards:

```text
SONY-1: 111b7019b53114c3
SONY-2: b559bdac76c898b0
```

Candidate D repeats exactly that plaintext.

Therefore:

```text
RUN0001 -> 8MB-MG-SONY-2
  because B -> b559bdac76c898b0

RUN0002 -> 8MB-MG-SONY-1
  because B -> 111b7019b53114c3

RUN0003 -> 64MB-MG-NOTSONY
  because the repeated identical F2 inputs diverge at the card-side outputs,
  reproducing the qualitative behavior already seen with Candidate A.
```

This mapping is also consistent with the complete Candidate-A/C history.

---

# 3. Port assignment

The three supplied runs did not all use the same logical memory-card port:

```text
RUN0001 / SONY-2   logical mc0 -> physical SECR/SIO2 port 2
RUN0002 / SONY-1   logical mc1 -> physical SECR/SIO2 port 3
RUN0003 / NONSONY  logical mc1 -> physical SECR/SIO2 port 3
```

This does not weaken the main Candidate-D result because the decisive comparison is within each single transaction: the same card, same port, same session and byte-identical F2 input are compared against two positions in that one run.

Earlier real-hardware testing already showed that, after the correct logical->physical port bridge, mc0/mc1 is not a determinant of final Sony card-bound Kbit/Kc.

---

# 4. RUN0001 = 8MB-MG-SONY-2

RTC evidence:

```text
2026-08-26T02:20:48
```

ICVPS2:

```text
6b156fd975d01290
```

Pre-card material:

```text
pre Kbit = 716338e2428c50b84dd40f4c6b78843f
pre Kc   = 716338e2428c50b84dd40f4c6b78843f
```

F2 vectors:

```text
A / Kbit0:
716338e2428c50b8 -> 890b39b28786304c

B / Kbit1:
4dd40f4c6b78843f -> b559bdac76c898b0

A / Kc0:
716338e2428c50b8 -> 890b39b28786304c

B / Kc1:
4dd40f4c6b78843f -> b559bdac76c898b0
```

Final values:

```text
Kbit = 890b39b28786304cb559bdac76c898b0
Kc   = 890b39b28786304cb559bdac76c898b0
```

**CONFIRMED:** repeated plaintext A produces byte-identical pre-card inputs and byte-identical F2 outputs in Kbit0 and Kc0.

**CONFIRMED:** repeated plaintext B produces byte-identical pre-card inputs and byte-identical F2 outputs in Kbit1 and Kc1.

**CONFIRMED cross-vector reproduction:** `B -> b559bdac76c898b0` is exactly the SONY-2 result previously observed for the same plaintext in Candidate C, even though Candidate C placed B at Kc0 while Candidate D places it at both Kbit1 and Kc1 and the MagicGate session is different.

This is strong real-hardware evidence that, for this tested Sony card/plaintext, the final F2 result is independent of Kbit/Kc position and session-wrapped input.

Processed header SHA-256:

```text
a1b0a9813f55b1492fa3223ffc387514d24fa54e25cdbd2fe332fc49adfe4051
```

---

# 5. RUN0002 = 8MB-MG-SONY-1

RTC evidence:

```text
2026-08-26T02:21:38
```

ICVPS2:

```text
686b1683e8f2f73c
```

Pre-card material:

```text
pre Kbit = 3da99d5782e7be586fcaaad36d22521a
pre Kc   = 3da99d5782e7be586fcaaad36d22521a
```

F2 vectors:

```text
A / Kbit0:
3da99d5782e7be58 -> 1b0d96b2404d2a10

B / Kbit1:
6fcaaad36d22521a -> 111b7019b53114c3

A / Kc0:
3da99d5782e7be58 -> 1b0d96b2404d2a10

B / Kc1:
6fcaaad36d22521a -> 111b7019b53114c3
```

Final values:

```text
Kbit = 1b0d96b2404d2a10111b7019b53114c3
Kc   = 1b0d96b2404d2a10111b7019b53114c3
```

**CONFIRMED:** repeated plaintext A produces the same pre-card input and same F2 output in Kbit0 and Kc0.

**CONFIRMED:** repeated plaintext B produces the same pre-card input and same F2 output in Kbit1 and Kc1.

**CONFIRMED cross-vector reproduction:** `B -> 111b7019b53114c3` exactly reproduces the prior SONY-1 Candidate-C vector for the same plaintext, despite a different KELF position and different session.

Processed header SHA-256:

```text
e8a16f02547854a9b4ddb032bb7b9fda7d0944b67fc2dead7f51492ab9d9c7fa
```

---

# 6. RUN0003 = 64MB-MG-NOTSONY

RTC evidence:

```text
2026-08-26T02:22:47
```

ICVPS2:

```text
dee682e3e7d2806e
```

Pre-card material:

```text
pre Kbit = 3cfefcaf0eef59cbf1f36053fdd6910c
pre Kc   = 3cfefcaf0eef59cbf1f36053fdd6910c
```

The key observation is that Candidate D produces byte-identical F2 inputs at the repeated positions:

```text
A input at Kbit0 = 3cfefcaf0eef59cb
A input at Kc0   = 3cfefcaf0eef59cb

B input at Kbit1 = f1f36053fdd6910c
B input at Kc1   = f1f36053fdd6910c
```

Yet the card returns different outputs:

```text
A / Kbit0:
3cfefcaf0eef59cb -> 14e38596a80cbac3

A / Kc0:
3cfefcaf0eef59cb -> 52ce42d5a1b1570a

B / Kbit1:
f1f36053fdd6910c -> 602b62fd8b6edf57

B / Kc1:
f1f36053fdd6910c -> 6c844a79b9127a96
```

Final values:

```text
Kbit = 14e38596a80cbac3602b62fd8b6edf57
Kc   = 52ce42d5a1b1570a6c844a79b9127a96
```

Processed header SHA-256:

```text
31e3dc6c809bc585c19a414057cc93f1c04380f21114d650fe3bc59e56f3d84f
```

## Decisive result

**CONFIRMED:** for NONSONY, card-side output is not a stateless function only of `(physical card, plaintext content key)`.

The stronger statement is possible because Candidate D controls the F2 boundary directly:

```text
same physical card
same transaction
same active session
same logical/physical port
same plaintext
same 8-byte pre-card/F2 input
same visible F2 command sequence 50 -> 51 -> 52 -> 53
but
Kbit-position output != Kc-position output
```

Therefore the divergence is introduced at or behind the card-side F2 boundary after the byte-identical input has been supplied.

**CONFIRMED:** hidden card-side context/state/history participates in the NONSONY transform.

**NOT YET DISTINGUISHED:** Candidate D alone cannot tell whether that hidden dependency is best described as:

- a fixed position-specific transform/key/IV for Kbit0/Kbit1/Kc0/Kc1;
- state carried from one F2 operation to the next;
- context established by preceding MagicGate commands;
- another deterministic internal implementation detail of this third-party card.

The external F2 command sequence itself is identical for each 8-byte half, so there is no explicit `Kbit`/`Kc` selector in the four commands recorded by the probe. Any semantic distinction is therefore internal state/context, not a different visible F2 opcode.

---

# 7. Relationship to Candidate A and Candidate C

Candidate A already gave an important warning sign. It used the same plaintext four times:

```text
3939393939393939
```

Both Sony cards returned one repeated result, while NONSONY returned four different stable results:

```text
Kbit0 -> 160a03515708a1e4
Kbit1 -> 24f64cdcc57f6c02
Kc0   -> 14d9895d4a9fd983
Kc1   -> 3b81b6bd30f92c74
```

Candidate C then showed that each NONSONY position remains stable across cold boots when given four distinct plaintexts.

Candidate D closes the largest ambiguity left by those datasets:

**CONFIRMED:** the NONSONY divergence is not caused by different session-wrapped inputs for the repeated positions. Candidate D makes those inputs byte-identical and the outputs still diverge.

For the Sony cards, Candidate D strengthens the opposite model:

**CONFIRMED for plaintext B on both tested Sony cards:** the same plaintext produces the same final card-bound output across different sessions and different KELF positions.

This is stronger than the original Candidate-A repeated-half observation because it reproduces a previously known Candidate-C vector in a new KELF layout.

---

# 8. ICVPS2

Candidate D returned three new transaction-specific ICVPS2 values:

```text
SONY-2  6b156fd975d01290
SONY-1  686b1683e8f2f73c
NONSONY dee682e3e7d2806e
```

No value duplicates the previously collected Candidate-A/C values in the project record.

**CONFIRMED:** Candidate D continues the existing observation that ICVPS2 is not a fixed KELF value, fixed card identifier, or fixed `(KELF, card)` result.

A single run per card does not by itself add a new statistical statement about repeated Candidate-D ICV stability. The stronger transaction-variability result remains supported by the earlier repeated-cold-boot datasets.

---

# 9. Binary comparison

All three `processed-header.bin` files are 136 bytes.

Pairwise differences are confined to:

```text
0x28..0x47  final card-bound Kbit/Kc
0x80..0x87  ICVPS2
```

The exact differing-byte count between the two Sony headers is 38 because two bytes of their independently generated card-bound outputs happen to match. This is not a structural difference: the only semantic ranges involved remain Kbit/Kc and ICVPS2.

Candidate D therefore preserves the same clean header separation observed in Candidate C.

---

# 10. Current model after Candidate D

## CONFIRMED - Sony cards

For both tested genuine Sony 8 MB cards, the evidence supports the following mapping for the tested plaintexts:

```text
plaintext content-key half
  -> transaction/session-variable MechaCon pre-card representation
  -> card F2/50-53
  -> stable card-specific representation
```

For plaintext `B = fedcba9876543210`, the output is reproduced across Candidate C and Candidate D despite different session state and KELF position.

The data are consistent with the public `ps3mca_tool` description of removing SessionKey wrapping and re-encrypting under card storage-key material.

## CONFIRMED - NONSONY

The unbranded 64 MB MagicGate-capable card is behaviorally different.

Its final F2 result depends on hidden card-side state/context in addition to plaintext/card identity. Candidate D proves this with byte-identical repeated F2 inputs in one transaction.

## INFERENCE

A useful implementation model for the third-party card is now:

```text
F2_output = G(card_internal_state_or_context, pre_card_input)
```

rather than the simpler Sony-compatible observation:

```text
F2_output ~= G(card, plaintext_content_key)
```

This notation is descriptive, not a claim about the exact internal cipher construction.

---

# 11. Next discrimination work

Candidate D proves hidden context/state participation on NONSONY but does not identify its form.

A high-information next KELF should complete a small plaintext-by-position matrix instead of adding more random values. For example, reuse A/B in complementary positions so the same plaintext is observed at every Kbit/Kc position across multiple KELFs.

That would test whether NONSONY behaves like four deterministic position-specific transforms or like a stateful sequence where the output also depends on preceding F2 history.

This is a **TEST HYPOTHESIS**, not required to accept the Candidate-D conclusion above.

Separately, dev.11's passive-auth experiment remains the current path for the independent ICVPS2/SessionKey question. Do not mix passive-auth instrumentation changes with a new KELF-position experiment in the same primary run series.
