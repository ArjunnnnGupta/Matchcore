#pragma once

#include "Types.hpp"

namespace matchcore {

// Deliberately plain data, no virtual functions: this struct is allocated
// out of a pool and copied/touched on the hot matching path, so it must stay
// trivially copyable with no vtable overhead.
struct Order {
    OrderId order_id{};

    // Trading account/client id. Used only for self-trade prevention (see
    // MatchingEngine's self-cross policy) — two orders from the same owner
    // must never trade against each other.
    uint64_t owner_id{};

    Timestamp timestamp_ns{};
    Side side{};
    OrderType type{};
    Price price{};
    Quantity quantity{};

    // A resting order's remaining tradable quantity. `quantity` above is the
    // originally submitted size and is left untouched for auditing/tests;
    // `remaining_quantity` is what matching logic consumes.
    Quantity remaining_quantity{};
};

}  // namespace matchcore
