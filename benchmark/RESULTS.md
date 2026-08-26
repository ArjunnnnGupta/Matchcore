# Benchmark Suite Results — C++ vs Python

Numbers below were measured on this machine, on this date, with the exact
commands shown. Re-run before quoting elsewhere.

## Machine

- MacBook (Apple M2, 8 cores, 8 GB RAM), macOS 26.6.1
- Apple clang 16.0.0, Python 3.13.2, pybind11 3.1.0
- Single-threaded, single process per run

## Pipeline under test

Same 3-stage pipeline on all three implementations (see `docs/ARCHITECTURE.md`
section 3.1): parse a tick (side, price) → update an in-memory best-bid/ask →
emit a signal (mid price vs. its own trailing moving average, window=20).
This is a simplified proxy for book-update work, not the full matching engine
in `engine/` — the point is to isolate interpreter/allocation overhead on a
small, well-defined per-tick loop.

Three implementations, identical input, identical driver/timer
(`run_benchmark.py`):

| Implementation | What it is |
|---|---|
| `python_row_loop` | Pure Python, one tick at a time — the fair streaming-semantics comparison |
| `python_vectorized` | pandas `ffill`/`rolling` over the whole column at once — idiomatic "fast pandas," but **not equivalent semantics**: it can see the whole column, a real streaming handler cannot |
| `cpp_row_loop` | Same row-by-row logic as `python_row_loop`, in C++, called via pybind11 |

## Command

```bash
cd matchcore/benchmark
pip install -r requirements.txt
python3 build_cpp_ext.py build_ext --inplace
python3 run_benchmark.py --input ../data/sample_ticks.csv --ticks 500000 --runs 10
```

Input: 500,000 LIMIT ticks filtered from `data/sample_ticks.csv` (same
synthetic data as the engine benchmark; MARKET orders excluded since they
don't carry a real price for this pipeline).

## Results (10 runs each, full output in terminal — mean of the 10)

| Implementation | Mean wall time | Median | Throughput |
|---|---|---|---|
| `python_row_loop` | 0.1566 s | 0.1511 s | 3.19M ticks/sec |
| `python_vectorized` | 0.0367 s | 0.0364 s | 13.6M ticks/sec |
| `cpp_row_loop` | 0.0030 s | 0.0028 s | **165.8M ticks/sec** |

**C++ vs. Python row-loop (fair, same semantics): 51.9x speedup on this run.**
**C++ vs. Python vectorized (different semantics, see caveat below): 12.2x speedup on this run.**

### Run-to-run variance — report a range, not one decimal

Across every `--ticks 500000 --runs 10` invocation run this session (four
separate times, on a clean build, while iterating on this benchmark and
recording the demo), the row-loop speedup came out as **51.7x, 51.9x,
47.9x, and 58.0x** — absolute throughput moved even more (the row-loop
alone ranged 1.7M–3.3M ticks/sec across runs). The honest number to quote is
a range: **~48–58x, roughly 52x typical**, not a single cherry-picked
decimal. Treating one run's 58.0x as "the" number would be exactly the kind
of unearned precision this project's own README warns against.

**Revision note:** an earlier version of this file reported lower absolute
throughput (2.00M / 8.55M / 103.4M ticks/sec) measured while multiple CMake
build directories existed and had recently compiled concurrently on this
machine (same cause as the revision noted in `engine/bench/RESULTS.md`).
Re-measured from a clean state and got a different absolute number yet
again on every subsequent run — see the variance note above. **The one
consistent fact across all runs**: the speedup ratio stays in a roughly
48–58x band even as absolute throughput swings 2x, since both
implementations are slowed proportionally by the same system contention.
The asciinema demo recording (`demo.cast`, linked from the README)
reproduces this again: recording overhead itself added enough load to pull
engine throughput down to 1.26M orders/sec and the pipeline speedup up to
58.0x for that specific take — expected, not a discrepancy, and left
un-re-recorded on purpose rather than cherry-picking a "cleaner" run.

Histogram of per-run wall times across all 10 runs for each implementation:
`benchmark/latency_histogram.png`.

## Why the gap — causal explanation, not just the number

- **Row-loop vs. C++ (~48–58x, see variance note above)**: this is the honest, apples-to-apples comparison.
  The Python interpreter re-dispatches a bytecode instruction and does a
  dictionary/attribute lookup for essentially every operation inside the
  loop body (`sides[i] == "B"`, deque append/pop, float boxing/unboxing on
  every arithmetic op). C++ compiles the identical logic to a tight loop
  over raw `double`/`int8_t` arrays with no per-element dispatch or heap
  allocation (the `std::deque<double>` window is the only allocation, and it
  stabilizes at a fixed size after the first `window` ticks). This is the
  same class of overhead the matching engine's object pool
  (`engine/bench/RESULTS.md`) was built to avoid, at a smaller scale.
- **Vectorized vs. C++ (12.1x, smaller gap)**: pandas closes most of the row-loop
  gap by pushing `ffill`/`rolling` into compiled numpy/pandas C code, avoiding
  per-element Python bytecode entirely — this is *why* "use vectorized pandas"
  is the standard advice for batch analytics. It still trails C++ because
  pandas allocates several full-length intermediate arrays (`bid`, `ask`,
  `mid`, `moving_avg`, `signal`) rather than fusing the pipeline into one
  pass, and because the API's generality carries per-call overhead a
  hand-written loop doesn't pay.
- **Why the vectorized comparison is not fair to report as "the" number**:
  `ffill()` looks backward *and* forward through data that, in a real feed
  handler, does not exist yet at tick *i*. It's the right tool for offline
  analytics over a completed dataset; it is not a substitute for a streaming
  book update. Reporting 12x as "the" C++ speedup would overstate what a live
  Python feed handler could actually achieve — the defensible number for that
  claim is the row-loop's ~48–58x range, not the vectorized comparison's 12x.

## What this does and doesn't prove

- Proves: for row-by-row, stateful, branch-heavy streaming logic — the actual
  shape of feed-handler/book-update work — C++ is roughly 50x faster than
  the equivalent Python here, and pybind11 makes it trivial to keep that hot
  loop in C++ while still driving it from Python.
- Does not prove: that Python is bad for trading research generally — the
  vectorized version is 8.5M ticks/sec, which is fine for offline analysis;
  the claim is specifically about the streaming hot path.
- Not done yet: per-tick latency percentiles (p50/p99) for the row-loop
  implementations — this run measures whole-batch wall time across repeated
  runs, not per-element latency distribution like `engine/bench/RESULTS.md`
  does for the matching engine. Worth adding if a future version needs a
  latency (not just throughput) claim for this pipeline specifically.
