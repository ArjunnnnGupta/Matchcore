#pragma once

#include <functional>

#include "Order.hpp"
#include "OrderBook.hpp"
#include "Types.hpp"

namespace matchcore {

struct Trade {
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp_ns;
};

enum class SubmitResult : uint8_t {
    FILLED,           // fully filled, nothing rests
    PARTIALLY_RESTED,  // partially filled, remainder rests on the book (LIMIT only)
    RESTED,            // no fill at all, full quantity rests (LIMIT only)
    CANCELLED,         // IOC/MARKET remainder that could not be filled, not rested
    SELF_CROSS_REJECTED,  // stopped by self-trade prevention; see class comment
};

// Owns the OrderBook and applies price-time-priority matching against it.
//
// Self-trade prevention (self-cross) policy — "cancel newest":
// while the incoming (aggressor) order is walking the opposite side, if the
// resting order at the front of the best opposite level has the same
// owner_id as the aggressor, matching stops immediately. Any quantity the
// aggressor has already filled against *other* owners stands; the
// aggressor's unfilled remainder is cancelled — never rested — even for a
// LIMIT order that would otherwise rest. The resting order that triggered
// the stop is left untouched on the book. This mirrors common exchange STP
// "cancel newest" behavior and avoids wash trades without needing to cancel
// or mutate resting orders belonging to other price-time priority.
class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit MatchingEngine(TradeCallback on_trade);

    // Submits a new order and applies price-time-priority matching.
    // `order` must outlive the call in the sense that its remaining_quantity
    // is mutated in place; if it ends up resting, the OrderBook stores the
    // pointer (ownership/lifetime is the caller's responsibility, typically
    // an object pool — see docs/ARCHITECTURE.md section 2.5).
    SubmitResult submit(Order* order);

    // Cancels a resting order by id. Returns true if found and cancelled.
    bool cancel(OrderId order_id);

    const OrderBook& book() const { return book_; }

private:
    // Returns true if matching should stop because the resting order at the
    // front of `side`'s best level shares owner_id with `aggressor`.
    bool self_cross_blocks(const Order& aggressor, Side resting_side) const;

    OrderBook book_;
    TradeCallback on_trade_;
};

}  // namespace matchcore
