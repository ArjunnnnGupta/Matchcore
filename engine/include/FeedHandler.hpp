#pragma once

#include <string>

#include "Order.hpp"

namespace matchcore {

// Parses CSV order records into Order structs. v1 reads a replay file for
// reproducible benchmarking (see docs/ARCHITECTURE.md section 2.4); a live
// binary-protocol parser would slot in here later behind the same interface.
class FeedHandler {
public:
    // Expected CSV columns, no quoting/escaping support (synthetic data only):
    //   order_id,owner_id,timestamp_ns,side,type,price,quantity
    // side is "B" or "S". type is "LIMIT", "MARKET", or "IOC". price is a
    // decimal string (e.g. "123.4567") converted to fixed-point ticks.
    //
    // Returns false for blank lines or the header row (line starting with
    // "order_id"), which callers should skip rather than treat as an order.
    // Throws std::invalid_argument on a malformed non-blank, non-header line.
    static bool parse_line(const std::string& line, Order& out);

    // Parses a decimal price string (e.g. "123.4567") into fixed-point ticks
    // at PRICE_SCALE, without going through float/double.
    static Price parse_price(const std::string& text);
};

}  // namespace matchcore
