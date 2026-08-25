# Candidate C real-hardware results

This document records the first complete real-hardware dataset for Candidate C, the ICV-enabled KELF vector with four deliberately distinct plaintext Kbit/Kc halves.

## Experiment input

Candidate C:

```text
SHA-256     87de11f092965122ea01bd6e3a908f5876cca0a42aaa49037bd63a39066d056d
flags       0x022e
header size 0x0088
```

Plaintext content-key halves:

```text
P0 = Kbit half0 = 0011223344556677
P1 = Kbit half1 = 8899aabbccddeeff
P2 = Kc   half0 = fedcba9876543210
P3 = Kc   half1 = 0f1e2d3c4b5a6978
```

Probe binary: hardware-successful dev.10 F2 trace build.

All nine supplied runs completed successfully through:

```text
SecrDownloadHeader
-> encrypted block
-> SecrDownloadGetKbit
-> F2/50-53 x2
-> SecrDownloadGetKc
-> F2/50-53 x2
-> SecrDownloadGetICVPS2 / 0x98
-> complete
```

All traced F2 records reported `mask=0x0f` and `failed=0x00`.

All nine runs returned the same BIT SHA-256:

```text
600486e0f71109358b650c05adbfa974bd71bf00055291b7475628b68265d89e
```

This is expected: the MechaCon returns the same decrypted BIT structure despite session/card-dependent mutation of the output Kbit/Kc and ICV slots in the processed header.

## Port assignment in the supplied archives

The archives were clearly labelled by physical card, but the selected probe port was not identical across all three groups:

```text
SONY-1   logical mc1 -> physical SECR/SIO2 port 3
SONY-2   logical mc0 -> physical SECR/SIO2 port 2
NONSONY  logical mc1 -> physical SECR/SIO2 port 3
```

This does not invalidate the card comparison. Earlier same-card Candidate-A testing already showed that mc0/mc1 does not alter final card-bound Kbit/Kc once the logical port is translated to the correct physical SIO2 channel. The Candidate-C dataset also reproduces stable final outputs despite the mixed port assignment.

---

# 1. 8MB-MG-SONY-1

All three cold boots used logical mc1 / physical port 3.

## RUN0001

```text
pre Kbit = 74602351ae984e8aa877ca1172efa82b
pre Kc   = be2d8b2f50318fc1b6a345c69a6aa446

P0/F2: 74602351ae984e8a -> 7754de423336eb85
P1/F2: a877ca1172efa82b -> cb52c9a3d18d2580
P2/F2: be2d8b2f50318fc1 -> 111b7019b53114c3
P3/F2: b6a345c69a6aa446 -> 52b1411707e22cd3

final Kbit = 7754de423336eb85cb52c9a3d18d2580
final Kc   = 111b7019b53114c352b1411707e22cd3
ICVPS2     = c27086e0c81d3976
```

## RUN0002

```text
pre Kbit = 250f6bd0aab6f58f9bf9f8b088535a6c
pre Kc   = aadcb80e2ca61b7121b202cc0a51ea43

P0/F2: 250f6bd0aab6f58f -> 7754de423336eb85
P1/F2: 9bf9f8b088535a6c -> cb52c9a3d18d2580
P2/F2: aadcb80e2ca61b71 -> 111b7019b53114c3
P3/F2: 21b202cc0a51ea43 -> 52b1411707e22cd3

final Kbit = 7754de423336eb85cb52c9a3d18d2580
final Kc   = 111b7019b53114c352b1411707e22cd3
ICVPS2     = 8af24f24885fbb32
```

## RUN0003

```text
pre Kbit = a6dcb2b8199ec7e016c7ad8a52b2765d
pre Kc   = 9fbd68830f0e0f7271b748289aa7b0a1

P0/F2: a6dcb2b8199ec7e0 -> 7754de423336eb85
P1/F2: 16c7ad8a52b2765d -> cb52c9a3d18d2580
P2/F2: 9fbd68830f0e0f72 -> 111b7019b53114c3
P3/F2: 71b748289aa7b0a1 -> 52b1411707e22cd3

final Kbit = 7754de423336eb85cb52c9a3d18d2580
final Kc   = 111b7019b53114c352b1411707e22cd3
ICVPS2     = b03072ccbb873223
```

### SONY-1 result

**CONFIRMED:** all four pre-card values vary between cold boots while each corresponding card-bound output remains byte-identical across all three runs.

Stable plaintext-to-card-bound vector:

```text
P0 0011223344556677 -> 7754de423336eb85
P1 8899aabbccddeeff -> cb52c9a3d18d2580
P2 fedcba9876543210 -> 111b7019b53114c3
P3 0f1e2d3c4b5a6978 -> 52b1411707e22cd3
```

---

# 2. 8MB-MG-SONY-2

All three cold boots used logical mc0 / physical port 2.

## RUN0004

```text
pre Kbit = a7e4ef928ad0c1821d51bd2e812b111e
pre Kc   = eed8a34ac34169ea43e41a7b1ef6ad08

P0/F2: a7e4ef928ad0c182 -> 4acf09a5ec5d10d4
P1/F2: 1d51bd2e812b111e -> 0589da87b42f05f5
P2/F2: eed8a34ac34169ea -> b559bdac76c898b0
P3/F2: 43e41a7b1ef6ad08 -> 53e0c7cabbd8797b

final Kbit = 4acf09a5ec5d10d40589da87b42f05f5
final Kc   = b559bdac76c898b053e0c7cabbd8797b
ICVPS2     = 9884cb26c278baca
```

## RUN0005

```text
pre Kbit = 7b675e546e6681e8a8ee583f965d83fd
pre Kc   = 54d0363ddca03d65fec78252ab6da470

P0/F2: 7b675e546e6681e8 -> 4acf09a5ec5d10d4
P1/F2: a8ee583f965d83fd -> 0589da87b42f05f5
P2/F2: 54d0363ddca03d65 -> b559bdac76c898b0
P3/F2: fec78252ab6da470 -> 53e0c7cabbd8797b

final Kbit = 4acf09a5ec5d10d40589da87b42f05f5
final Kc   = b559bdac76c898b053e0c7cabbd8797b
ICVPS2     = c1fd0ca14f4f1166
```

## RUN0006

```text
pre Kbit = bdcf3830cb052000780ab787c6f138d8
pre Kc   = 51544f688c9d3703634472a9cc956b0b

P0/F2: bdcf3830cb052000 -> 4acf09a5ec5d10d4
P1/F2: 780ab787c6f138d8 -> 0589da87b42f05f5
P2/F2: 51544f688c9d3703 -> b559bdac76c898b0
P3/F2: 634472a9cc956b0b -> 53e0c7cabbd8797b

final Kbit = 4acf09a5ec5d10d40589da87b42f05f5
final Kc   = b559bdac76c898b053e0c7cabbd8797b
ICVPS2     = 961cc941cb1a1bdb
```

### SONY-2 result

Stable plaintext-to-card-bound vector:

```text
P0 0011223344556677 -> 4acf09a5ec5d10d4
P1 8899aabbccddeeff -> 0589da87b42f05f5
P2 fedcba9876543210 -> b559bdac76c898b0
P3 0f1e2d3c4b5a6978 -> 53e0c7cabbd8797b
```

Again, every pre-card value changes between cold boots while all four final outputs remain stable.

---

# 3. 64MB-MG-NOTSONY

All three cold boots used logical mc1 / physical port 3.

## RUN0007

```text
pre Kbit = c438c4b04dacf98867d11a8f327a03cb
pre Kc   = 7b0dd205b4b3f435b1d0021df0452898

P0/F2: c438c4b04dacf988 -> 494d3dac3c3aa5ed
P1/F2: 67d11a8f327a03cb -> 396e1f0232b1565f
P2/F2: 7b0dd205b4b3f435 -> bf18fae977c1ac15
P3/F2: b1d0021df0452898 -> 37a0d08129d19946

final Kbit = 494d3dac3c3aa5ed396e1f0232b1565f
final Kc   = bf18fae977c1ac1537a0d08129d19946
ICVPS2     = d3aab9d907373e57
```

## RUN0008

```text
pre Kbit = dd8b9af7cc3d7f1787325c6b397db91a
pre Kc   = 9dae1bfc1ed60283d0f86d319b4d771a

P0/F2: dd8b9af7cc3d7f17 -> 494d3dac3c3aa5ed
P1/F2: 87325c6b397db91a -> 396e1f0232b1565f
P2/F2: 9dae1bfc1ed60283 -> bf18fae977c1ac15
P3/F2: d0f86d319b4d771a -> 37a0d08129d19946

final Kbit = 494d3dac3c3aa5ed396e1f0232b1565f
final Kc   = bf18fae977c1ac1537a0d08129d19946
ICVPS2     = 8d2df4a6a375d851
```

## RUN0009

```text
pre Kbit = 65cf36b5ff86efda91dc6f425352bd92
pre Kc   = 4c69d292966a4bb0341a07dceebb694f

P0/F2: 65cf36b5ff86efda -> 494d3dac3c3aa5ed
P1/F2: 91dc6f425352bd92 -> 396e1f0232b1565f
P2/F2: 4c69d292966a4bb0 -> bf18fae977c1ac15
P3/F2: 341a07dceebb694f -> 37a0d08129d19946

final Kbit = 494d3dac3c3aa5ed396e1f0232b1565f
final Kc   = bf18fae977c1ac1537a0d08129d19946
ICVPS2     = 17cd67c71c22faf6
```

### NONSONY result

Stable plaintext-to-card-bound vector:

```text
P0 0011223344556677 -> 494d3dac3c3aa5ed
P1 8899aabbccddeeff -> 396e1f0232b1565f
P2 fedcba9876543210 -> bf18fae977c1ac15
P3 0f1e2d3c4b5a6978 -> 37a0d08129d19946
```

---

# 4. Cross-run binary comparison

Within each physical-card group, the three 136-byte `processed-header.bin` files differ at exactly:

```text
0x80..0x87
```

These eight bytes are the ICVPS2 slot.

Within one card group, the final card-wrapped Kbit/Kc at `0x28..0x47` are therefore completely stable across the three cold boots.

Across different physical cards, representative processed headers differ only at:

```text
0x28..0x47  final Kbit/Kc
0x80..0x87  ICVPS2
```

All other header bytes remain identical.

This is an unusually clean separation of card-dependent key material from transaction-dependent ICVPS2 for a byte-identical input KELF.

---

# 5. Main conclusions

## CONFIRMED: one session, four independent pre-card equations

Candidate C solves Candidate A's repeated-plaintext limitation. Within each successful transaction the four known plaintext content-key blocks produce four distinct pre-card values, consistent with one active session transform applied independently to each 8-byte half.

The exact pre-card values change on every cold boot.

## CONFIRMED: F2 removes session variability from the final key material

For a fixed physical card and fixed plaintext content-key block, the output of the complete stock `F2/50 -> F2/51 -> F2/52 -> F2/53` sequence is stable across all three cold boots despite different session-wrapped inputs.

This is direct real-hardware evidence of the boundary described by public `ps3mca_tool` source: the card receives content-key material encrypted under the current session key, removes that session wrapping and returns a storage/card-bound representation.

## CONFIRMED: the storage/card-bound mapping differs by physical card

For every P0..P3 plaintext block, SONY-1, SONY-2 and NONSONY return different stable outputs.

Therefore the final mapping is not merely a console/KELF transform. The physical card participates materially in the stored key representation.

## CONFIRMED: ICVPS2 remains transaction-variable

All nine Candidate-C ICVPS2 captures are transaction-specific in this dataset. They change while the card-bound Kbit/Kc remain stable within each card group.

This continues to separate ICVPS2 from the final storage-wrapped key representation.

---

# 6. Important NONSONY observation

Candidate A used the same plaintext content-key block (`3939393939393939`) for Kbit0, Kbit1, Kc0 and Kc1.

The two Sony cards produced identical final outputs for all four identical plaintext positions, while the 64 MB unbranded card produced four different stable final values:

```text
Candidate A / NONSONY
P = 3939393939393939

position Kbit0 -> 160a03515708a1e4
position Kbit1 -> 24f64cdcc57f6c02
position Kc0   -> 14d9895d4a9fd983
position Kc1   -> 3b81b6bd30f92c74
```

This is qualitatively different from both Sony cards.

Candidate C does not by itself distinguish whether this behavior comes from:

- separate storage keys / IVs selected by position;
- state carried between successive F2 operations;
- a clone-specific transformation not matching Sony's apparently stateless repeated-block behavior;
- another implementation detail correlated with Kbit/Kc half order.

This is now the strongest next card-side question.

---

# 7. Candidate D test hypothesis

The next controlled KELF should intentionally repeat two plaintext blocks in alternating positions:

```text
P0 = A
P1 = B
P2 = A
P3 = B
```

Recommended values:

```text
A = 0123456789abcdef
B = fedcba9876543210

Kbit = A || B
Kc   = A || B
```

One transaction then gives two same-plaintext pairs under the exact same session:

```text
Kbit0 plaintext A vs Kc0 plaintext A
Kbit1 plaintext B vs Kc1 plaintext B
```

### TEST HYPOTHESIS D1: stateless card mapping

If F2 behaves as a stateless function of `(physical card, plaintext content-key block)` after removing session wrapping:

```text
final(Kbit0/A) == final(Kc0/A)
final(Kbit1/B) == final(Kc1/B)
```

This is expected for the two tested Sony cards based on Candidate A.

### TEST HYPOTHESIS D2: position/state-dependent clone mapping

If the NONSONY card's Candidate-A behavior is genuine position/state dependence, at least one repeated pair should remain different despite identical plaintext and a shared session:

```text
final(Kbit0/A) != final(Kc0/A)
and/or
final(Kbit1/B) != final(Kc1/B)
```

Because dev.10 also records the pre-card input, Candidate D will simultaneously verify that identical plaintext blocks under one session produce identical pre-card values before F2. This makes any divergence at F2/53 unambiguously card-side.

One successful cold boot per card is sufficient for the first D1/D2 discrimination because Candidate C already established cold-boot stability. A second run is useful only to reproduce a surprising result.

---

# 8. ICVPS2 / session-key next step

Candidate C provides four known plaintext / pre-card ciphertext pairs under the same unknown session key in every run. Public `ps3mca_tool` models these pre-card values as single-DES encryption under the current MagicGate SessionKey.

This is enough to strongly constrain and validate a captured session key, but not practical evidence for recovering a 56-bit DES key by naive CPU brute force.

The next probe-side experiment should therefore remain passive:

- preserve dev.10's successful KELF/F2 path exactly;
- instrument `SecrAuthCard()` to remember the most recent *naturally occurring successful* authentication transcript;
- do **not** call `SecrAuthCard()` from the KELF path;
- do **not** call `mcGetInfo`, `F3`, or any reset/replay command;
- if a successful auth occurs after the instrumented SECRMAN is loaded, copy CardIV, CardMaterial, CardNonce, MechaChallenge1/2/3 and CardResponse1/2/3 into unused RPC evidence space alongside the normal ICV result;
- if no natural auth is observed, report `auth_trace_present=false` without changing the KELF transaction.

A negative result would itself establish that the session used by the successful KELF path was created before the probe's post-launch IOP reset, which is plausible when the homebrew itself was launched through an FMCB/MagicGate boot path.

This passive experiment avoids repeating the dev.8/dev.9 mistake of perturbing the security state we are trying to observe.
