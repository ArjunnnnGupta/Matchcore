# Architecture

## 1. System overview

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
                              writes CSV + histogram

  ─────────────────────── separate track ───────────────────────

  Same conceptual pipeline (parse → book update → signal)
  implemented twice for comparison:

     Python path:  pandas/numpy vectorized tick processor
     C++ path:     pybind11 module, called from the same Python
                    driver so timing is apples-to-apples

  run_benchmark.py drives both, plots latency distributions
```

## 2. Core components (engine/)

### 2.1 Order
Plain data struct — deliberately not polymorphic (no vtable overhead on the hot path).
```
struct Order {
    uint64_t order_id;
    uint64_t timestamp_ns;
    Side side;          // BUY / SELL
    OrderType type;      // LIMIT / MARKET / IOC
    Price price;          // fixed-point integer, NOT float (see Design Decisions)
    Quantity quantity;
};
```

### 2.1.1 Price representation (Phase 0 decision — locked before OrderBook code)
`Price` is `int64_t` representing integer "ticks", where `1 tick = 0.0001` currency units (scale factor `10000`). A displayed price like `123.4567` is stored as the integer `1234567`.

- **Why 10000 (4 decimal places):** covers standard equity/futures tick sizes (down to $0.0001) without needing per-instrument configurable scale for v1. Simpler than a per-symbol tick-size table; documented as a future extension if multi-asset support is added.
- **Why integer, not float/double:** float/double comparisons (`==`, `<`) on prices accumulate rounding error across millions of arithmetic operations and comparisons — two prices that are "the same" after arithmetic can compare unequal, which is silently wrong for order matching and price-level bucketing. Integer ticks make price equality and ordering exact.
- **Conversion boundary:** the fixed-point scale (`PRICE_SCALE = 10000`) lives in `Types.hpp` as a single constant. Parsing (`FeedHandler`) converts decimal strings to ticks; any human-facing output (bench reports, CSV replay) converts back. No float ever enters the matching hot path.
- **Overflow headroom:** `int64_t` gives room up to ~9.2 * 10^14 in price units at this scale — far beyond any realistic instrument price, so no overflow handling is needed on the price field itself.

### 2.2 OrderBook
- Two sides (bids, asks), each a sorted structure keyed by price level.
- Within a price level: a FIFO queue of orders (time priority).
- Data structure choice: `std::map<Price, PriceLevel, Compare>` for the first working version (log n insert/erase, simple, correct). Documented upgrade path to a flat array / intrusive linked-list-based book (O(1) best-bid/ask access) once correctness is proven — this progression is itself a good interview story ("I started with std::map for correctness, profiled it, found X% of time in rebalancing, and moved to Y").
- `PriceLevel` holds a `std::deque<Order*>` or intrusive list for O(1) pop-front on execution and O(1) append on new order.

### 2.3 MatchingEngine
- Owns the OrderBook, exposes `submit(Order)`, `cancel(order_id)`.
- Matching logic: price-time priority. New aggressive order walks the opposite book from best price outward, consuming resting liquidity until filled or book exhausted (for IOC: remainder is cancelled, not resting).
- Emits `Trade` events (buy_order_id, sell_order_id, price, quantity, timestamp) to a callback/queue rather than printing inline — keeps the hot path free of I/O.

### 2.4 FeedHandler
- Reads a tick/order source (CSV replay for v1; documented extension point for a binary protocol parser later — this is where "ITCH-like" parsing work would slot in if you extend toward project idea #2).
- Converts raw records into `Order` structs with zero unnecessary copies (parse directly into pre-allocated buffers).

### 2.5 Memory management
- Pre-allocated object pool for `Order` objects (fixed-size pool, freelist) — avoids `new`/`delete` on the hot path once the engine is warmed up.
- No STL containers that allocate per-operation inside the matching loop where avoidable (e.g., avoid `std::vector::push_back` reallocation storms — reserve capacity upfront).

### 2.6 Benchmark harness (bench/)
- Wraps each `submit()` call with `std::chrono::steady_clock` timestamps (or `rdtsc` for tighter measurement if you want to go further).
- Collects latencies into a vector, post-processes into percentiles — do NOT print per-order in the hot loop (I/O will dominate and lie to you about the engine's real speed).
- Reports: total orders processed, wall time, throughput (orders/sec), latency histogram (p50/p90/p99/p99.9/max).

## 3. Benchmark suite (benchmark/) — pairing project #5

### 3.1 Shared task definition
Both implementations do the *same* three-stage pipeline on the *same* input data, so the comparison is fair:
1. Parse a tick record (symbol, price, size, timestamp) from a line/row.
2. Update an in-memory best-bid/best-ask state (a simplified version of the book update, not full matching — keeps the comparison focused).
3. Emit a trivial signal (e.g., mid-price crossing a moving average) — enough compute to be representative, not so much that it swamps the parse/update cost.

### 3.2 Python path
- `pandas.read_csv` + vectorized operations where possible, and a row-by-row loop variant too (since real feed handlers are inherently sequential/stateful — vectorized pandas is not a fair proxy for streaming logic, so include both and be honest in the write-up about which comparison is fair).

### 3.3 C++ path
- Same logic, compiled, exposed via `pybind11` so it's called from the identical Python driver/timer — this removes "different measurement code" as a confound.

### 3.4 Output
- Latency histograms (matplotlib) for both paths, side by side.
- A short written note in `benchmark/RESULTS.md` on where the gap comes from (allocation, interpreter overhead, branch prediction, cache locality) — this analysis is worth more in an interview than the number itself.

## 4. Design decisions worth defending in an interview

| Decision | Why | What they might push back on |
|---|---|---|
| Fixed-point integer prices, not float/double | Avoids float rounding errors in price comparisons — real exchanges do this | "Why not just round?" → rounding accumulates error across millions of comparisons |
| `std::map` first, then optimize | Correctness before speed; profile before micro-optimizing | "Why not start with the fast structure?" → premature optimization risk, and the profiling story is the actual signal |
| Object pool instead of raw new/delete | Removes allocator overhead + fragmentation from the hot path | "Why not just use a good allocator?" → valid alternative, worth knowing tcmalloc/jemalloc exist as the "give it to someone else" answer |
| Single-threaded core first | Correctness and clean latency measurement before concurrency complexity | "Real engines are multi-threaded/lock-free" → true, documented as Phase 2 stretch, not overclaimed as done |
| CSV replay instead of live feed | Reproducible benchmarking, no exchange connectivity needed for a resume project | Be upfront this is a simulation, not a production feed handler |

## 5. Explicit non-goals (say this out loud in interviews — it shows judgment)

- Not a production-grade exchange (no persistence, no fault tolerance, no regulatory compliance).
- Not multi-asset-class from day one — one instrument type (equity/futures-style single-price-level book) first.
- Not distributed/networked — single process, in-memory only for v1.
- Not claiming FPGA/kernel-bypass-level latency — this is a well-engineered single-threaded C++ system, not a co-located HFT rig, and the write-up should say so plainly.
