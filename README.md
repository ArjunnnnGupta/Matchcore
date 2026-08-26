# MatchCore

A price-time-priority limit order book matching engine written in C++, plus a companion benchmark suite comparing C++ and Python tick-processing pipelines on latency and throughput.

Built to demonstrate the systems-level skills relevant to electronic market making, matching engine design, and low-latency infrastructure — the core technical surface of proprietary trading firms.

## What this is

Two components under one repo:

1. **`engine/`** — a standalone C++ matching engine. Accepts limit, market, IOC, and cancel orders; maintains a price-time-priority order book; executes trades; replays historical/synthetic tick data at speed; reports throughput (orders/sec) and per-order latency (p50/p99/p99.9).
2. **`benchmark/`** — the same core tick-processing pipeline (parse tick → update book → emit signal) implemented three ways: Python row-loop, Python vectorized (pandas), and C++ (exposed to Python via pybind11). Head-to-head latency/throughput comparison with a real histogram, not just wall-clock prints. (No flamegraphs — `perf` isn't available on macOS; see the honest-gaps notes in `CHECKPOINT.md`.)

## Why these two together

The engine proves you can build the thing exchanges/market makers run. The benchmark proves you understand *why* firms drop to C++ where it matters and stay in Python where it doesn't — which is a more senior, more hireable claim than either project alone.

## Quick start

Or run [`demo.sh`](demo.sh) for the whole flow below in one shot (build → test → both benchmarks). A recorded run is at [`demo.cast`](demo.cast) (play with `asciinema play demo.cast`, or `asciinema upload` it yourself if you want a shareable link — not done here). Note: the throughput/speedup numbers visible in that specific recording are lower/different than the headline numbers below — recording itself adds CPU overhead, which is the same system-contention effect documented in `engine/bench/RESULTS.md` and `benchmark/RESULTS.md`. That's expected, not a discrepancy; it wasn't re-recorded to chase a cleaner-looking take.

```bash
# Engine (CMake root is matchcore/, not matchcore/engine/)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
ctest                    # run the unit tests (Debug build recommended for this — see below)
./engine/matchcore_bench --input ../data/sample_ticks.csv --orders 1000000

# Benchmark suite
cd ../benchmark
pip install -r requirements.txt
python3 build_cpp_ext.py build_ext --inplace
python3 run_benchmark.py --input ../data/sample_ticks.csv --ticks 500000 --runs 10
```

Note: `ctest` works from a Release build too, but a plain `-DCMAKE_BUILD_TYPE=Debug` build (no `-march=native`, sanitizer-friendlier) is what's actually used for `matchcore_tests` throughout development — see `engine/bench/RESULTS.md` for why Release is reserved for the benchmark binary specifically.

## Headline numbers

| Metric | Target | Measured |
|---|---|---|
| Matching engine throughput | ≥ 500K orders/sec (single-threaded, Release build) | **~1.3M–2.4M orders/sec** (varies with system load — see caveat below) |
| Matching engine p99 latency | < 5 µs per order | **250–420 ns** |
| Feed handler tick-to-book | < 2 µs per tick, zero heap allocs on hot path | not yet measured (Phase 4 covers the tick pipeline's throughput, not this specific per-tick claim) |
| C++ vs Python speedup (benchmark suite, row-loop/streaming) | report actual measured ratio, expect 20–100x depending on stage | **~48–58x across runs, ~52x typical** |
| C++ vs Python speedup (vs. vectorized pandas) | — | ~12x (different semantics — not a fair streaming comparison, see caveat in `benchmark/RESULTS.md`) |

Both benchmarks show real run-to-run variance on this machine — driven by
whatever else is competing for CPU at measurement time (other build
directories compiling, or even the recording overhead of the demo below).
Ranges above are measured, not padding for safety; single-decimal numbers
from one lucky run are a real way to embarrass yourself when an interviewer
asks you to reproduce them live. See `engine/bench/RESULTS.md` and
`benchmark/RESULTS.md` for every individual run's raw output and root cause.

Engine numbers measured on an Apple M2 (macOS 26.6.1, Apple clang 16.0.0), 1M synthetic orders, `-O3 -march=native` — see [`engine/bench/RESULTS.md`](engine/bench/RESULTS.md). Benchmark-suite numbers measured on the same machine, 500K ticks x 10 runs — see [`benchmark/RESULTS.md`](benchmark/RESULTS.md). Re-run both yourself before quoting these numbers elsewhere.

Never write a number into this README, your resume, or an interview answer that you have not personally measured on your own machine. Reviewers at a trading firm will ask "how did you measure that" as the very next question.

## Resume bullet (draft)

> Built a C++ limit-order-book matching engine supporting price-time priority matching, IOC/cancel semantics, and tick replay, sustaining 1.3M+ orders/sec throughput at sub-microsecond p99 latency; benchmarked the tick-processing hot path against an equivalent Python pipeline (pybind11), quantifying a ~50x latency reduction over a fair streaming-semantics comparison.

Deliberately conservative (lower bound of the measured range, not the best single run) — safe to say out loud and reproduce live if asked.

## Architecture

```
                        ┌─────────────────────────────────────────┐
                        │              MatchCore Engine             │
                        │                                             │
  Tick/Order  ──────►   │  FeedHandler ──► OrderBook ──► MatchingEngine │ ──► Trade events
  Replay Source          │   (parse)         (state)        (match)      │      Latency stats
                        │                                             │
                        └─────────────────────────────────────────┘
                                          │
                                          ▼
                              Benchmark harness (bench/)
                              records per-order latency,
                              computes p50/p99/p99.9,
                              writes throughput report

  ─────────────────────── separate track (benchmark/) ───────────────────────

  Same conceptual pipeline (parse → book update → signal)
  implemented three ways for comparison:

     Python row-loop:   pure Python, one tick at a time (fair streaming proxy)
     Python vectorized: pandas ffill/rolling (idiomatic, but not streaming-faithful)
     C++ path:           pybind11 module, called from the same Python driver
                          so timing is apples-to-apples

  run_benchmark.py drives all three, plots latency histograms
```

Full design rationale (data structure choices, fixed-point pricing, the
self-cross policy, non-goals) is in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Repo layout

```
matchcore/
├── CMakeLists.txt              # project root — cmake is run from matchcore/, not engine/
├── demo.sh                      # build -> test -> both benchmarks, in one script
├── demo.cast                     # asciinema recording of demo.sh (play with `asciinema play`)
├── CHECKPOINT.md                  # fast-reference state summary, not part of the "story"
├── engine/
│   ├── include/                    # headers: Types, Order, OrderBook, MatchingEngine, FeedHandler, ObjectPool
│   ├── src/                         # implementations
│   ├── tests/                        # Catch2 unit tests (14 cases / 69 assertions)
│   ├── bench/
│   │   ├── replay_bench.cpp            # benchmark harness + tick replay driver
│   │   └── RESULTS.md                   # measured numbers, exact commands, honest gaps
│   └── CMakeLists.txt
├── benchmark/
│   ├── python_pipeline/pipeline.py       # row-loop + vectorized pandas implementations
│   ├── cpp_pipeline/pipeline.cpp          # pybind11 module wrapping the hot loop
│   ├── build_cpp_ext.py                    # builds the pybind11 extension
│   ├── run_benchmark.py                     # drives all three, plots latency_histogram.png
│   ├── RESULTS.md                            # measured numbers + causal explanation
│   └── requirements.txt
├── data/
│   ├── generate_orders.py       # synthetic order/tick generator
│   ├── sample_ticks.csv          # generated output (not hand-written)
│   └── README.md                  # data provenance
└── docs/
    ├── ARCHITECTURE.md
    ├── TECHSTACK.md
    ├── PLAN.md
    ├── SKILLS.md
    └── STAR_STORIES.md
```

## Status

All 5 phases complete (see `docs/PLAN.md` for the phase-by-phase checklist,
all synced to what's actually true — including the one item honestly left
unchecked). Matching engine, order book, feed handler, object pool, and
replay benchmark harness are built and tested (69 assertions / 14 test
cases, all passing); the pybind11 C++ vs. Python benchmark suite is built;
both are measured with real numbers reported as ranges (see "Headline
numbers" above and both `RESULTS.md` files for why). Terminal demo recorded
(`demo.cast`), 4 STAR interview stories written (`docs/STAR_STORIES.md`).
Phase 4 has no automated tests by explicit request — quick-turnaround
benchmark code, not correctness-critical matching logic. Optional stretch
goals (multi-threading, flat-array book, market-making bot) are not started.
