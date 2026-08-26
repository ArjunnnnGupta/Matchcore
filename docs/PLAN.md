# Build Plan

Scoped for a student with strong CP/algorithmic background but building a first real systems project. Sequenced so you have a working, demoable artifact at the end of every phase — never a half-built engine with nothing to show.

## Phase 0 — Setup (0.5–1 day) — DONE
- [x] Repo scaffold matching the layout in README.md
- [x] CMake project — confirmed toolchain works end to end (skipped the literal placeholder `main.cpp` step and verified via the real Catch2 test suite instead, which is a strictly stronger check)
- [x] Catch2/GoogleTest wired in with one trivial passing test (`tests/test_placeholder.cpp`)
- [x] Decide fixed-point price representation and write it down in ARCHITECTURE.md before writing OrderBook code (§2.1.1: int64 ticks, scale 10000)

**Exit criteria:** ✅ `cmake .. && make && ctest` runs green (14/14 tests, including the placeholder).

## Phase 1 — Order + OrderBook core (2–4 days) — DONE
- [x] `Order` struct (plus an `owner_id` field beyond the original spec, needed for self-trade prevention in Phase 2)
- [x] `OrderBook` with `std::map<Price, PriceLevel>` for both sides (`PriceLevel` uses `std::list<Order*>`, not `std::deque`, so cancel-by-id iterators stay valid — deque invalidates all iterators on push)
- [x] `add_order`, `cancel_order`
- [x] Unit tests: insert maintains price ordering, FIFO within a level, cancel removes correctly, best-bid/best-ask queries correct
- [x] Print/visualize book state as a sanity check (`OrderBook::dump`)

**Exit criteria:** ✅ proven by `tests/test_orderbook.cpp`.

## Phase 2 — Matching engine (3–5 days) — DONE
- [x] `MatchingEngine::submit()`
- [x] Partial fill handling
- [x] IOC semantics
- [x] Market order handling
- [x] Self-cross handling — policy chosen and documented: "cancel-newest" (aggressor's unfilled remainder is cancelled, resting order untouched); see `MatchingEngine.hpp` class comment
- [x] Trade event emission via callback
- [x] Unit tests covering: full fill, partial fill, multi-level sweep, IOC cancel-remainder, market order, self-cross (two cases: pure self-cross, and partial fill against another owner before hitting self-cross), cancel

**Exit criteria:** ✅ proven by `tests/test_matching_engine.cpp` (69 assertions across both test files).

## Phase 3 — Feed handler + replay + benchmark harness (2–4 days) — DONE (one item honestly unverified)
- [x] Synthetic tick/order generator (`data/generate_orders.py`)
- [x] C++ CSV parser → `Order` structs (`FeedHandler`)
- [x] Benchmark driver: replay N orders, record per-order latency with `steady_clock`, compute throughput + percentiles (`engine/bench/replay_bench.cpp`)
- [ ] Object pool for `Order` allocation — **implemented** (`ObjectPool<Order>`), but the Valgrind/Massif verification was **not done**: Valgrind isn't available on this machine (macOS). The claim is "avoids allocation by construction," not "measured and confirmed" — see `docs/STAR_STORIES.md` gaps section. Re-run on Linux before claiming this checkbox.
- [x] Release build flags (`-O3 -march=native`), re-measured (multiple times, after discovering the first measurement was skewed by concurrent builds — see `engine/bench/RESULTS.md` revision note)

**Exit criteria:** ✅ real, reproducible numbers in `engine/bench/RESULTS.md` with exact commands (~2.2–2.35M orders/sec, p99 250–290ns).

## Phase 4 — Benchmark suite: C++ vs Python (project #5) (2–3 days) — DONE (no automated tests, by explicit request)
- [x] Shared 3-stage pipeline (parse tick → best-bid/ask update → mid-vs-moving-average signal) defined identically in both languages
- [x] Python row-loop version AND vectorized pandas version, explicitly labeled and the semantic difference documented (vectorized `ffill` sees future data a streaming handler can't)
- [x] C++ implementation of the same row-loop logic, exposed via pybind11
- [x] `run_benchmark.py` driving all three with identical input data and timing methodology
- [x] Matplotlib histogram (`benchmark/latency_histogram.png`)
- [x] `benchmark/RESULTS.md`: numbers + causal explanation (interpreter dispatch overhead, allocation, why the vectorized comparison isn't reported as "the" number)

**Exit criteria:** ✅ chart + defensible write-up in `benchmark/RESULTS.md`. Measured: 51.7–51.9x speedup vs. Python row-loop (fair comparison), 12.1–12.2x vs. vectorized pandas (different semantics, labeled as such).

## Phase 5 — Polish for resume/interview (1–2 days) — DONE
- [x] README numbers filled in with real measured values (revised once after catching a measurement error — see `engine/bench/RESULTS.md`)
- [x] Architecture diagram in README (reused ARCHITECTURE.md's ASCII)
- [x] Terminal demo: `demo.sh` (build → test → both benchmarks) + `demo.cast` recorded via `asciinema rec demo.cast -c ./demo.sh` — linked from the README
- [x] 4 STAR-format stories written in `docs/STAR_STORIES.md`, each grounded in something that actually happened this session (not the object-pool/Valgrind story suggested in SKILLS.md, since that measurement was never run — see the gaps section in that doc)

**Exit criteria:** ✅ someone unfamiliar can run `./demo.sh` (or the Quick Start commands) and reproduce the numbers above in well under 10 minutes.

## Optional stretch (only after Phase 5, only if time remains)
- [ ] Multi-threaded order ingestion with a lock-free SPSC queue feeding a single-threaded matching core (keeps matching logic simple/correct while adding concurrency at the boundary — a defensible, realistic design choice)
- [ ] Flat-array/intrusive-list order book to replace `std::map`, re-benchmark, document the delta
- [ ] Simple market-making bot strategy layered on top, measure its simulated P&L/inventory risk on replayed data

## Rough timeline
Total: **~3 weeks** at a steady pace alongside coursework (Phases 0–3 in week 1, Phase 4 in week 2, Phase 5 + buffer/debugging in week 3). Compress to ~10 focused days if you can dedicate full days during a break.
