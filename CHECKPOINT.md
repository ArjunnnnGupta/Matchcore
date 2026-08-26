# Checkpoint — 2026-08-26

## Status: Phases 0–5 all done. Project is interview-ready per PLAN.md's exit criteria.

## Decisions locked in
- C++17, Catch2 (FetchContent v3.6.0), self-cross policy = **cancel-newest** (aggressor's unfilled remainder is cancelled, resting order untouched) — see `MatchingEngine.hpp` class comment.
- Price = `int64_t` ticks, scale 10000 (4 decimals) — see `docs/ARCHITECTURE.md` §2.1.1.
- `PriceLevel` uses `std::list<Order*>` (not `std::deque`) so cancel-by-id iterators stay valid.
- Self-cross needs an `owner_id` field on `Order` (added beyond the original doc spec) to distinguish trader accounts.
- CMake root is `matchcore/CMakeLists.txt` (single `build/` dir), NOT `matchcore/engine/` — `engine/CMakeLists.txt` has no `project()` call and can't be built standalone. The README originally documented the wrong path; fixed.
- No git repo initialized (user chose not to).
- No frontend/dashboard — deliberately out of scope, `docs/TECHSTACK.md` explicitly calls out web frameworks as scope creep for this project.

## Build/test/bench (single build dir, matches README exactly)
```bash
cd matchcore && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j && ctest
./engine/matchcore_bench --input ../data/sample_ticks.csv --orders 1000000
```
14 test cases / 69 assertions, all passing. Or just run `./demo.sh` for the whole flow including the Phase 4 benchmark suite.

## Measured numbers — report as ranges, not single decimals (important)
Both benchmarks showed real run-to-run variance this session, traced to CPU
contention (concurrent build dirs, or even the demo recording's own
overhead) — not flaky code. Always quote the range + cause, not one lucky
run:
- Engine throughput: **1.26M–2.4M orders/sec** across runs (target ≥500K — comfortably clear even at the low end). p99 latency: **250–420ns** (target <5µs).
- Benchmark suite C++ vs Python speedup: **48x–58x** (row-loop, fair streaming comparison), ~52x typical. vs. vectorized pandas: ~12x (different semantics, not the headline number).
- Full raw output for every run + root-cause analysis: `engine/bench/RESULTS.md`, `benchmark/RESULTS.md`.
- Resume bullet (README) deliberately uses conservative lower-bound numbers (1.3M+ orders/sec, ~50x) — safe to say out loud and reproduce live.

## Phase 4: benchmark suite (pybind11 C++ vs Python)
```bash
cd matchcore/benchmark
python3 build_cpp_ext.py build_ext --inplace
python3 run_benchmark.py --input ../data/sample_ticks.csv --ticks 500000 --runs 10
```
Shared pipeline (parse tick -> update best-bid/ask -> mid-vs-moving-average signal) in three forms: `python_row_loop`, `python_vectorized` (pandas ffill/rolling), `cpp_row_loop` (pybind11). No automated tests for this piece — skipped by explicit request, quick-turnaround benchmark code not correctness-critical matching logic. Chart at `benchmark/latency_histogram.png`.

## Phase 5: polish (all done)
- README: architecture diagram added (ASCII reused from ARCHITECTURE.md), headline numbers as ranges with explained variance, resume bullet uses conservative numbers.
- `demo.sh`: full build→test→both-benchmarks flow, safe to re-run (wipes and recreates `build/`).
- `demo.cast`: real asciinema recording of `demo.sh`, made via `asciinema rec demo.cast -c ./demo.sh` (local file only, never uploaded/published anywhere). Its on-screen numbers (1.26M orders/sec, 58.0x speedup) are lower/different than the headline ranges because recording adds its own CPU overhead — documented in README next to the link, not hidden, and not re-recorded to chase a cleaner take.
- `docs/STAR_STORIES.md`: 4 stories, all grounded in things that actually happened this session (measurement-error correction, fixed-point pricing decision, self-cross policy + its tests, fair-vs-unfair benchmark comparison). Explicitly does NOT include the Valgrind-object-pool story from `SKILLS.md` since that measurement was never run — see its own gaps section.
- `docs/PLAN.md`: checkboxes synced to reality, including the one honestly left unchecked (Valgrind verification of the object pool).

## Known environment issues (not code bugs)
- `cmake` and `asciinema` weren't installed; both installed via `brew install`.
- ASan/UBSan crash on this machine even on a trivial 2-line program (`sanitizer_malloc_mac.inc:189` init-order bug) — confirmed environment-level (isolated outside the repo), documented in `engine/bench/RESULTS.md`. Re-try on Linux.
- `catch_discover_tests` needed `DISCOVERY_MODE PRE_TEST` — default mode runs the test binary at build time, which crashed under ASan and would silently delete the binary on failure.
- Root `CMakeLists.txt` needs `enable_testing()` (not just in `engine/CMakeLists.txt`) or `ctest` run from `build/` finds nothing.

## Honest gaps remaining (not blockers, just don't overclaim these)
- Valgrind/Massif allocation-profiling claim for the object pool — unverified (no Valgrind on macOS).
- `perf` profiling of the p99.9/max latency gap — unverified (Linux-only).
- No per-tick latency percentiles for the Phase 4 pipeline (only whole-batch throughput across repeated runs).
- `std::map`→flat-array OrderBook migration — documented as a future direction in ARCHITECTURE.md, never implemented or profiled. Don't claim it happened.
- Optional stretch goals (multi-threaded ingestion, flat-array book, market-making bot) — not started, not required for interview-readiness.

## Repo layout
Project lives in `matchcore/` (subfolder of this dir). The original loose top-level `.md` files (`ARCHITECTURE.md`, `PLAN.md`, etc.) have been **deleted** — content lives in `matchcore/docs/` and `matchcore/README.md` now; nothing left duplicated at the top level.
