#include "OrderBook.hpp"

#include <cassert>

namespace matchcore {

void OrderBook::add_order(Order* order) {
    if (order->side == Side::BUY) {
        PriceLevel& level = bids_[order->price];
        level.orders.push_back(order);
        level.total_quantity += order->remaining_quantity;
        locations_[order->order_id] = {Side::BUY, order->price, std::prev(level.orders.end())};
    } else {
        PriceLevel& level = asks_[order->price];
        level.orders.push_back(order);
        level.total_quantity += order->remaining_quantity;
        locations_[order->order_id] = {Side::SELL, order->price, std::prev(level.orders.end())};
    }
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto loc_it = locations_.find(order_id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation& loc = loc_it->second;

    if (loc.side == Side::BUY) {
        auto level_it = bids_.find(loc.price);
        assert(level_it != bids_.end());
        PriceLevel& level = level_it->second;
        level.total_quantity -= (*loc.it)->remaining_quantity;
        level.orders.erase(loc.it);
        if (level.orders.empty()) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(loc.price);
        assert(level_it != asks_.end());
        PriceLevel& level = level_it->second;
        level.total_quantity -= (*loc.it)->remaining_quantity;
        level.orders.erase(loc.it);
        if (level.orders.empty()) {
            asks_.erase(level_it);
        }
    }

    locations_.erase(loc_it);
    return true;
}

void OrderBook::pop_front(Side side) {
    if (side == Side::BUY) {
        assert(!bids_.empty());
        auto level_it = bids_.begin();
        PriceLevel& level = level_it->second;
        Order* front = level.orders.front();
        level.total_quantity -= front->remaining_quantity;
        locations_.erase(front->order_id);
        level.orders.pop_front();
        if (level.orders.empty()) {
            bids_.erase(level_it);
        }
    } else {
        assert(!asks_.empty());
        auto level_it = asks_.begin();
        PriceLevel& level = level_it->second;
        Order* front = level.orders.front();
        level.total_quantity -= front->remaining_quantity;
        locations_.erase(front->order_id);
        level.orders.pop_front();
        if (level.orders.empty()) {
            asks_.erase(level_it);
        }
    }
}

Order* OrderBook::peek_front(Side side) const {
    if (side == Side::BUY) {
        if (bids_.empty()) return nullptr;
        return bids_.begin()->second.orders.front();
    } else {
        if (asks_.empty()) return nullptr;
        return asks_.begin()->second.orders.front();
    }
}

void OrderBook::reduce_front_quantity(Side side, Quantity amount) {
    if (side == Side::BUY) {
        assert(!bids_.empty());
        bids_.begin()->second.total_quantity -= amount;
    } else {
        assert(!asks_.empty());
        asks_.begin()->second.total_quantity -= amount;
    }
}

std::optional<Price> OrderBook::best_price(Side side) const {
    if (side == Side::BUY) {
        if (bids_.empty()) return std::nullopt;
        return bids_.begin()->first;
    } else {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }
}

bool OrderBook::empty(Side side) const {
    return side == Side::BUY ? bids_.empty() : asks_.empty();
}

void OrderBook::dump(std::ostream& os) const {
    os << "-- ASKS (best to worst) --\n";
    for (auto it = asks_.rbegin(); it != asks_.rend(); ++it) {
        os << "  " << (static_cast<double>(it->first) / PRICE_SCALE) << " x "
           << it->second.total_quantity << " (" << it->second.orders.size() << " orders)\n";
    }
    os << "-- BIDS (best to worst) --\n";
    for (const auto& [price, level] : bids_) {
        os << "  " << (static_cast<double>(price) / PRICE_SCALE) << " x "
           << level.total_quantity << " (" << level.orders.size() << " orders)\n";
    }
}

}  // namespace matchcore
