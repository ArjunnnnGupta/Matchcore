# STAR Stories — MatchCore

Grounded strictly in what was actually built and measured (see `git`-free
history in `CHECKPOINT.md` and the two `RESULTS.md` files). Some stories
suggested in `SKILLS.md` (Valgrind-verified object pool, a profiled
std::map→flat-array migration) are **not included here** because they were
never actually run — see "Gaps to be upfront about" at the bottom instead of
overclaiming them in an interview.

## 1. "Tell me about a time you caught and corrected a measurement error."

**Situation:** The matching engine benchmark initially reported ~1.35M
orders/sec at 417ns p99 latency, recorded in `engine/bench/RESULTS.md`.

**Task:** Before treating that number as final (going into a README/resume),
verify it was actually reproducible from a clean environment — per this
project's own standing rule ("never write a number you haven't personally
measured").

**Action:** Deleted all build directories and rebuilt strictly from the
README's documented quick-start commands, as a stand-in for what an
unfamiliar reviewer would do. The re-measured throughput came back
~2.2–2.35M orders/sec — a 60%+ jump. Rather than pick whichever number
looked better, traced the discrepancy: the original measurement was taken
while three separate CMake build directories (Debug, Release, and an
ASan attempt) had recently been compiling concurrently on an 8-core
machine, which was still contending for CPU during the timed run.

**Result:** Re-ran the benchmark 3x from a single clean build with no
concurrent compilation to confirm the corrected number was stable
(2.20M–2.34M orders/sec across runs), and documented the root cause and both
numbers in `RESULTS.md`'s revision note rather than silently overwriting the
old figure. The lesson generalized further than expected: the same
contention effect showed up again later, in the overhead of recording the
terminal demo itself (`demo.cast`), which pulled engine throughput back down
to 1.26M orders/sec on that specific take. Across four separate timed runs
this session, absolute throughput swung roughly 2x (1.26M–2.4M orders/sec)
while the *relative* C++/Python speedup ratio moved less (48x–58x) —
noisier under load than hoped, but nowhere near as noisy as the absolute
number. Rather than re-recording the demo to chase a cleaner-looking take,
or reporting whichever single run's ratio looked most impressive, reported a
range (~48–58x, ~52x typical) in `benchmark/RESULTS.md` and explained why
next to the demo link in the README — a range with the cause explained is
more defensible under interview follow-up than one confident decimal.

## 2. "Tell me about a design decision you'd defend."

**Situation:** `Order.price` needed a concrete representation before any
order book code could be written.

**Task:** Decide between a floating-point price (simpler to write) and a
fixed-point integer representation, and be able to defend it under
technical pushback.

**Action:** Chose `int64_t` ticks at a 10,000x scale (4 decimal places) —
documented in `docs/ARCHITECTURE.md` §2.1.1 *before* writing `OrderBook`,
per the project's own Phase 0 rule. The concrete argument: `double`
comparisons (`==`, `<`) used for price-level bucketing and matching
accumulate rounding error across millions of arithmetic operations, so two
prices that are conceptually identical after computation can compare
unequal — silently wrong for a price-time-priority book, where price
equality determines which level an order lands in. Integers make price
equality exact by construction.

**Result:** Every price comparison in `OrderBook`/`MatchingEngine` is exact
integer comparison, with no epsilon-tolerance hacks anywhere in the matching
logic. When asked "why not just round?", the answer is that rounding doesn't
fix the underlying problem — it just changes how much error accumulates
before it causes an incorrect comparison, it doesn't eliminate it.

## 3. "Tell me about a decision you made and how you proved it worked."

**Situation:** ARCHITECTURE.md explicitly called out self-crossing (an
order matching its own resting order) as a policy the implementer had to
choose and defend, without prescribing an answer.

**Task:** Pick a self-trade-prevention policy and prove it does what it
claims, including in the partial-fill case.

**Action:** Chose "cancel-newest": if the resting order at the front of the
best opposite-side level shares an owner with the incoming aggressor, that
aggressor's *unfilled remainder* is cancelled outright — never rested, even
if it's a LIMIT order — while the resting order is left completely
untouched. Wrote two dedicated test cases, not one: the obvious case (100%
self-cross, zero trades), and the harder case where the aggressor first
fills against a *different* owner's resting order before hitting its own
order at a worse price — proving fills against non-self stand and only the
self-crossing remainder gets cancelled.

**Result:** Both tests pass (`test_matching_engine.cpp`), and the policy
mirrors real exchange "cancel newest" STP behavior rather than inventing
something bespoke — a defensible answer if pushed on "why not cancel the
resting order instead?" (because that would penalize price-time priority
established by orders that arrived first).

## 4. "Tell me about making sure a comparison was fair."

**Situation:** Phase 4 required comparing a Python tick-processing pipeline
against the same logic in C++. The obvious "fast Python" version — pandas
`ffill()` + `rolling()` — was tempting to use as *the* Python baseline
because it produces the best Python number.

**Task:** Decide whether that comparison would actually be honest.

**Action:** `ffill()` looks both backward and forward through the entire
column at once. A real streaming feed handler processing tick *i* has never
seen tick *i+1* — it cannot forward-fill. So the vectorized pandas version
and the C++ row-loop are not solving the same problem under the same
constraints; reporting only the vectorized-vs-C++ gap as "the" speedup would
overstate what a live Python service could achieve. Built both a
row-by-row Python version (true streaming semantics, directly comparable to
the C++ implementation, which mirrors it exactly) and the vectorized
version, and reported both numbers with the caveat spelled out in
`benchmark/RESULTS.md` rather than picking the more flattering one.

**Result:** Two honest numbers instead of one impressive-but-misleading
one: ~48–58x (row-loop vs. C++, apples-to-apples, ~52x typical across
repeated runs) and ~12x (vectorized vs. C++, different semantics, labeled
as such). The README's resume-bullet draft uses a deliberately conservative
"~50x" — the low end of the measured range, not the best single run — so
it holds up if reproduced live in an interview.

## Gaps to be upfront about (don't oversell these in an interview)

- The object pool (`ObjectPool<Order>`) is designed to avoid per-order heap
  allocation, but that claim is **not Valgrind/Massif-verified** — Valgrind
  isn't available on this machine (macOS). The honest claim is "avoids
  allocation by construction," not "measured and confirmed."
- `std::map` is still the live `OrderBook` implementation. The documented
  upgrade path to a flat-array/intrusive book (`docs/ARCHITECTURE.md` §2.2)
  has **not been implemented or profiled** — don't claim a profiling-driven
  migration that didn't happen.
- ASan/UBSan was attempted and is blocked by a confirmed environment-level
  bug on this machine (crashes even on a trivial two-line program, isolated
  outside the repo to prove it wasn't MatchCore's code) — a real debugging
  story in its own right, but the correct framing is "blocked, root-caused,
  not yet resolved," not "verified."
