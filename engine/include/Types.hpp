#pragma once

#include <cstdint>

namespace matchcore {

// See docs/ARCHITECTURE.md section 2.1.1 for the rationale behind fixed-point
// integer prices instead of float/double.
using Price = int64_t;
using Quantity = uint64_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;  // nanoseconds since an arbitrary epoch

constexpr Price PRICE_SCALE = 10000;

enum class Side : uint8_t {
    BUY,
    SELL,
};

enum class OrderType : uint8_t {
    LIMIT,
    MARKET,
    IOC,
};

}  // namespace matchcore
