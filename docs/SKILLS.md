# Skills This Project Demonstrates — and How to Talk About Them

## Mapping to what Futures First / Hertshten Group actually do
From their own language: **electronic market making with automated, low-latency liquidity provision using proprietary algorithms and co-located infrastructure**; **consolidated tick/level-2 market data feeds**; **proprietary matching engines, risk-control systems, automated backtesting platforms, latency-optimized software stacks**.

| What they said | What MatchCore proves you understand |
|---|---|
| "proprietary matching engines" | You built one — price-time priority, partial fills, IOC, self-cross policy |
| "low-latency liquidity provision" | You measured latency (p50/p99/p99.9), not just "it's fast" — and you know what a microsecond costs |
| "consolidated tick/level-2 feeds" | Your FeedHandler parses tick data into book state — same conceptual job, smaller scope |
| "automated backtesting platforms" | Your replay harness is a minimal backtest driver: feed historical/synthetic data through the engine deterministically |
| "latency-optimized software stacks" | Object pooling, avoiding hot-path allocation, Release build flags, profiling with perf/Valgrind — you can explain *why*, not just *that* |
| "pre-trade risk checks / margining controls" (Quora description) | Not built in v1 — honest gap. If asked, this is your natural "what I'd build next" answer (ties to project idea #3 from your options list) |

## Core technical skills (concrete, defensible)
- **Systems-level C++**: manual memory management tradeoffs, STL container selection under performance constraints, fixed-point arithmetic for financial correctness, RAII, avoiding UB (verified with sanitizers).
- **Data structures under real constraints**: price-time priority book (map + FIFO queues), and the reasoning for *when* to move off `std::map` — this is a stronger CP-to-systems bridge story than a generic LeetCode answer, since you have your own CP background to draw the contrast from.
- **Performance measurement discipline**: you don't claim a number without a percentile breakdown and a reproducible command. This separates you from most student resumes that just say "optimized for speed."
- **Cross-language systems thinking (pybind11)**: knowing where the Python/C++ boundary should sit in a real trading research-to-production pipeline — this is precisely the shape of tooling used at quant shops, not a toy integration.
- **Testing discipline for correctness-critical code**: matching logic is exactly the kind of code where an untested edge case (partial fill accounting, self-cross) is a real financial bug in production — your test suite is evidence you think this way.

## STAR stories to prep from this project (write these out fully before interviews)
1. **"Tell me about a time you optimized something."** → Object pool for Order allocation: measured heap allocations with Valgrind/Massif before and after, quantified the throughput/latency improvement, explain the tradeoff (memory reserved upfront vs. allocator overhead removed).
2. **"Tell me about a design decision you'd defend."** → Fixed-point integer prices instead of floats — explain the rounding-error argument concretely with a small example.
3. **"Tell me about a time you started simple and iterated."** → `std::map`-based book first, documented upgrade path, profiling-before-optimizing discipline.
4. **"Tell me about the limits of your own work."** → The explicit non-goals list in ARCHITECTURE.md — no concurrency in v1, no real exchange connectivity, single-threaded — said plainly and confidently, not defensively. Confidence about scope is itself a signal of maturity.
5. **"Why C++ here and Python there?"** → Benchmark suite results directly, with the causal explanation (interpreter overhead, allocation, cache locality) from `benchmark/RESULTS.md`.

## Interview framing — the narrative arc
Lead with the *systems* framing, not the *finance* framing: "I wanted to understand what actually happens inside the software that runs a derivatives market, so I built a simplified matching engine and measured it properly, then quantified when C++ is worth the complexity versus Python." This positions you as someone who investigates infrastructure out of genuine interest — which is the exact "principal assets are its people... intellectual curiosity" language from their own about page — rather than someone who built a project purely to pattern-match a JD.

## Gaps to be upfront about (don't oversell)
- No real market connectivity or live data — synthetic/replay only, and that's fine to say directly.
- No FPGA/kernel-bypass/co-location-level optimization — this is well-engineered single-threaded C++, not HFT-grade infrastructure, and claiming otherwise will not survive a technical follow-up question.
- No multi-asset support in v1 — single instrument type, documented as a clear extension point.
