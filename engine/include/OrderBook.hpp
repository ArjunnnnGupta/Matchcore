#pragma once

#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <unordered_map>

#include "Order.hpp"
#include "Types.hpp"

namespace matchcore {

// FIFO queue of resting orders at a single price level (time priority).
// std::list is used instead of std::deque so that iterators handed out to
// resting orders stay valid across insertions/removals elsewhere in the
// list (deque invalidates ALL iterators on push_back/push_front, which would
// break the O(1) cancel-by-id lookup below).
struct PriceLevel {
    std::list<Order*> orders;
    Quantity total_quantity{};
};

// Price-time-priority limit order book for a single instrument.
//
// Two sides, each keyed by price with opposite orderings so that
// `begin()` on either map is always the best price for that side:
//   bids: descending (highest price = best bid)
//   asks: ascending  (lowest price = best ask)
//
// v1 uses std::map for both sides. See docs/ARCHITECTURE.md section 2.2 for
// the documented upgrade path to a flat-array/intrusive book once this is
// profiled.
class OrderBook {
public:
    using Bids = std::map<Price, PriceLevel, std::greater<Price>>;
    using Asks = std::map<Price, PriceLevel, std::less<Price>>;

    // Inserts a resting order into the book. Caller retains ownership of the
    // Order (typically backed by an object pool); OrderBook only stores the
    // pointer and bookkeeping needed to find/remove it later.
    void add_order(Order* order);

    // Removes an order by id. Returns true if it was found and removed.
    bool cancel_order(OrderId order_id);

    // Removes the resting order currently at the front of the best level on
    // `side` (used by the matching engine once that order is fully filled).
    // Precondition: !empty(side).
    void pop_front(Side side);

    // Returns the resting order at the front of the best level on `side`, or
    // nullptr if that side is empty.
    Order* peek_front(Side side) const;

    // Reduces the front level's aggregate `total_quantity` bookkeeping by
    // `amount`, without popping the front order. Used after a partial fill
    // of the resting order at the front of the best level, whose own
    // `remaining_quantity` the matching engine mutates directly through the
    // pointer returned by peek_front(). Precondition: !empty(side).
    void reduce_front_quantity(Side side, Quantity amount);

    std::optional<Price> best_price(Side side) const;
    bool empty(Side side) const;

    const Bids& bids() const { return bids_; }
    const Asks& asks() const { return asks_; }

    // Simple text dump of book state (price -> total resting quantity per
    // level), best-to-worst on each side. Sanity-check tool, not a UI.
    void dump(std::ostream& os) const;

private:
    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order*>::iterator it;
    };

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, OrderLocation> locations_;
};

}  // namespace matchcore
