# Matching Engine — Replay Benchmark Results

Numbers below were measured on this machine, on this date, with the exact
commands shown. Re-run them yourself before quoting these in an interview —
see PLAN.md's standing rule against unmeasured numbers.

## Machine

- MacBook (Apple M2, 8 cores, 8 GB RAM), macOS 26.6.1
- Apple clang 16.0.0 (arm64-apple-darwin25.6.0)
- Single-threaded, single process, no other significant load during the run

## Build

```bash
cd matchcore
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j matchcore_bench
```

Use a fresh/dedicated build directory for benchmarking — building multiple
CMake configurations concurrently (e.g. a second Debug/ASan tree compiling
at the same time) measurably skews these numbers on an 8-core machine; see
the revision note below.

`CMAKE_BUILD_TYPE=Release` applies `-O3 -march=native` (see `engine/CMakeLists.txt`).
`-march=native` tunes for *this* CPU — numbers are not guaranteed to transfer
unmodified to another machine; that is expected and worth saying plainly in
an interview.

## Data

1,000,000 synthetic orders, deterministic seed:

```bash
cd matchcore/data
python3 generate_orders.py --output sample_ticks.csv --orders 1000000 --seed 42
```

Random-walk mid price around $100, mixed LIMIT (~85%) / IOC (~10%) / MARKET
(~5%) orders, 200 distinct owner ids (so self-trade-prevention rejections
occur naturally in the replay, not just in unit tests). Fully synthetic — see
`data/README.md`.

## Command

```bash
cd matchcore/build
./engine/matchcore_bench --input ../data/sample_ticks.csv --orders 1000000
```

## Results (representative of 3 consecutive runs on a clean single build — see raw output below)

| Metric | Measured | README target |
|---|---|---|
| Throughput | **~2.2–2.35M orders/sec** | ≥ 500K orders/sec |
| p50 latency | **83 ns** | — |
| p90 latency | **125 ns** | — |
| p99 latency | **250–291 ns** | < 5 µs |
| p99.9 latency | **375–542 ns** | — |
| max latency | 33–365 µs (single outlier per run, likely a std::map rebalance / allocator page fault) | — |
| Trades executed | 915,172 / 1,000,000 orders submitted | — |

Both throughput and p99 latency clear the README's targets by a wide margin
on this hardware — expected, since those targets were set conservatively
before any measurement existed, per PLAN.md Phase 3.

**Revision note:** an earlier version of this file reported ~1.35M orders/sec
/ 417ns p99, measured while three separate CMake build directories
(`build/`, `build-release/`, a since-deleted `build-asan/`) existed and had
recently been compiling concurrently on this 8-core machine. Re-measured
from a single clean build with no concurrent build activity; the numbers
below are the corrected, reproducible figures — always re-run this yourself
rather than trusting either number blindly.

### Raw output (3 runs, same command/data, clean single build)

```
orders submitted:  1000000
trades executed:   915172
wall time:         0.454397 s
throughput:        2200719 orders/sec
latency p50:       83 ns
latency p90:       125 ns
latency p99:       291 ns
latency p99.9:     542 ns
latency max:       364500 ns

orders submitted:  1000000
trades executed:   915172
wall time:         0.429295 s
throughput:        2329402 orders/sec
latency p50:       83 ns
latency p90:       125 ns
latency p99:       250 ns
latency p99.9:     375 ns
latency max:       33084 ns

orders submitted:  1000000
trades executed:   915172
wall time:         0.427233 s
throughput:        2340645 orders/sec
latency p50:       83 ns
latency p90:       125 ns
latency p99:       250 ns
latency p99.9:     375 ns
latency max:       34541 ns
```

## What's implemented vs. not (be upfront about this)

- Object pool (`ObjectPool<Order>`, see `include/ObjectPool.hpp`) is used by
  the bench driver so order allocation is a single upfront `std::vector`
  allocation, not per-order `new`. **Not yet independently verified with
  Valgrind/Massif** — that tool isn't available on this machine (macOS); the
  claim here is "the design avoids per-order heap allocation by
  construction," not "confirmed via allocation profiler." Re-run under
  Valgrind on Linux before claiming the stronger version of this in an
  interview.
- `perf`-based profiling (also Linux-only) has not been run. The p99.9/max
  gap above is a reasonable candidate for what `perf` would explain
  (`std::map` node allocation/rebalancing on the first insert at a new price
  level is the leading hypothesis, not yet confirmed).
- AddressSanitizer was attempted and is currently **blocked by a confirmed
  environment bug, not skipped**: on this machine (macOS 26.6.1, Apple clang
  16.0.0), ASan crashes with `AddressSanitizer: CHECK failed:
  sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)"` even for a
  trivial `new`/`delete` program with no MatchCore code involved — confirmed
  by isolating it to a two-line reproduction outside this repo. This is a
  known class of macOS/ASan-runtime incompatibility, not a defect in the
  engine. Re-run `-fsanitize=address,undefined` on Linux (or an older Xcode
  toolchain) before claiming ASan-verified memory safety.
