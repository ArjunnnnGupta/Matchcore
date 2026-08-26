# Tech Stack

## Core languages
| Tool | Version | Use |
|---|---|---|
| C++ | C++17 (or 20 if your toolchain supports it) | Matching engine, feed handler, hot-path benchmark pipeline |
| Python | 3.11+ | Orchestration, plotting, pandas pipeline, pybind11 glue |

## C++ build & tooling
| Tool | Use |
|---|---|
| CMake (≥3.20) | Build system — standard in industry, worth learning properly rather than raw Makefiles |
| GCC or Clang | Compiler — use `-O3 -march=native` for Release benchmark builds, document this explicitly since it affects reproducibility on other machines |
| Catch2 or GoogleTest | Unit tests for OrderBook/MatchingEngine correctness (test price-time priority, partial fills, IOC cancel-remainder, edge cases like self-crossing) |
| perf (Linux) | Profiling — find where time actually goes before optimizing |
| Valgrind / Massif | Memory correctness + allocation profiling (confirm the "zero heap allocs on hot path" claim is actually true, not assumed) |
| AddressSanitizer / UBSan | Catch memory bugs early — cheap to add, expected of anyone claiming production-style C++ |

## C++ libraries (keep minimal — dependencies are a liability in a systems project)
| Library | Use |
|---|---|
| `<chrono>` (STL) | Timing/latency measurement |
| `<map>`, `<deque>` (STL) | Initial OrderBook implementation (see Architecture doc — deliberately simple first) |
| pybind11 | Exposing the C++ benchmark pipeline to Python for the head-to-head comparison |
| fmt (optional) | Faster, safer formatted output for logging outside the hot path |

## Python side
| Tool | Use |
|---|---|
| pandas / numpy | Python reference pipeline in the benchmark suite |
| pybind11 / setuptools | Building the C++ extension module |
| matplotlib | Latency histograms, throughput comparison charts |
| pytest | Python-side test coverage for the benchmark orchestration code |

## Data
| Source | Use |
|---|---|
| Synthetic tick generator (write this yourself — a Python script producing realistic-ish price/size/timestamp sequences with configurable volatility) | Primary data source — avoids licensing/access issues with real exchange feeds |
| Public sample datasets (e.g., a small NSE/CME sample if freely available, or Kaggle tick datasets) | Optional realism layer — clearly label provenance and license in `data/README.md` |

## Why this stack (say this if asked)
- C++17/20 + CMake + Catch2 is the actual toolchain used at most systems-heavy trading shops — not "modern flashy" but "what they run."
- Deliberately avoiding heavier frameworks (no Boost unless a specific need arises, no networking libraries yet) keeps the binary lean and keeps you able to explain every line — a from-scratch matching engine loses its point if it's mostly library calls.
- pybind11 for the benchmark suite specifically (not just "using Python and C++ separately") because it's the same technique used in real quant/trading shops to keep research in Python while pushing hot loops to C++ — this is a stronger signal than two disconnected scripts.

## What NOT to add (resist scope creep)
- No web framework / REST API — this is a systems project, not a web app; adding Flask/FastAPI dilutes the signal.
- No database — CSV/in-memory is fine and appropriate for the scope.
- No multi-threading in v1 — add only after the single-threaded engine is correct, tested, and profiled (see PLAN.md Phase 4 stretch).
- No cloud deployment — this runs on your laptop; that's the right scope for a resume project, not a weakness.
