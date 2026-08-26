#include "FeedHandler.hpp"

#include <array>
#include <cctype>
#include <stdexcept>

namespace matchcore {

namespace {

std::array<std::string, 7> split_fields(const std::string& line) {
    std::array<std::string, 7> fields;
    size_t field_index = 0;
    size_t start = 0;
    for (size_t i = 0; i <= line.size() && field_index < fields.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            fields[field_index++] = line.substr(start, i - start);
            start = i + 1;
        }
    }
    if (field_index != fields.size()) {
        throw std::invalid_argument("FeedHandler: expected 7 CSV fields, got " +
                                     std::to_string(field_index) + " in line: " + line);
    }
    return fields;
}

}  // namespace

Price FeedHandler::parse_price(const std::string& text) {
    const size_t dot = text.find('.');
    std::string int_part = (dot == std::string::npos) ? text : text.substr(0, dot);
    std::string frac_part = (dot == std::string::npos) ? "" : text.substr(dot + 1);

    // Fixed-point scale is 4 decimal digits (PRICE_SCALE = 10000); pad or
    // truncate the fractional part to exactly that width.
    if (frac_part.size() < 4) {
        frac_part.append(4 - frac_part.size(), '0');
    } else {
        frac_part.resize(4);
    }

    const Price int_value = std::stoll(int_part);
    const Price frac_value = frac_part.empty() ? 0 : std::stoll(frac_part);
    return int_value * PRICE_SCALE + frac_value;
}

bool FeedHandler::parse_line(const std::string& line, Order& out) {
    if (line.empty()) return false;
    if (line.rfind("order_id", 0) == 0) return false;  // header row

    const auto fields = split_fields(line);

    out.order_id = std::stoull(fields[0]);
    out.owner_id = std::stoull(fields[1]);
    out.timestamp_ns = std::stoull(fields[2]);

    if (fields[3] == "B") {
        out.side = Side::BUY;
    } else if (fields[3] == "S") {
        out.side = Side::SELL;
    } else {
        throw std::invalid_argument("FeedHandler: unknown side '" + fields[3] + "'");
    }

    if (fields[4] == "LIMIT") {
        out.type = OrderType::LIMIT;
    } else if (fields[4] == "MARKET") {
        out.type = OrderType::MARKET;
    } else if (fields[4] == "IOC") {
        out.type = OrderType::IOC;
    } else {
        throw std::invalid_argument("FeedHandler: unknown order type '" + fields[4] + "'");
    }

    out.price = parse_price(fields[5]);
    out.quantity = std::stoull(fields[6]);
    out.remaining_quantity = out.quantity;

    return true;
}

}  // namespace matchcore
