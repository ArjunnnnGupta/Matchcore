#include "MatchingEngine.hpp"

#include <algorithm>
#include <utility>

namespace matchcore {

MatchingEngine::MatchingEngine(TradeCallback on_trade) : on_trade_(std::move(on_trade)) {}

bool MatchingEngine::self_cross_blocks(const Order& aggressor, Side resting_side) const {
    const Order* resting = book_.peek_front(resting_side);
    return resting != nullptr && resting->owner_id == aggressor.owner_id;
}

SubmitResult MatchingEngine::submit(Order* order) {
    const Side opposite = (order->side == Side::BUY) ? Side::SELL : Side::BUY;
    const Quantity original_remaining = order->remaining_quantity;
    bool self_cross_hit = false;

    while (order->remaining_quantity > 0 && !book_.empty(opposite)) {
        const Price best = *book_.best_price(opposite);

        bool crosses;
        if (order->type == OrderType::MARKET) {
            crosses = true;
        } else if (order->side == Side::BUY) {
            crosses = order->price >= best;
        } else {
            crosses = order->price <= best;
        }
        if (!crosses) break;

        if (self_cross_blocks(*order, opposite)) {
            self_cross_hit = true;
            break;
        }

        Order* resting = book_.peek_front(opposite);
        const Quantity fill_qty = std::min(order->remaining_quantity, resting->remaining_quantity);
        const Price trade_price = resting->price;  // resting/passive order sets the trade price

        Trade trade{
            order->side == Side::BUY ? order->order_id : resting->order_id,
            order->side == Side::BUY ? resting->order_id : order->order_id,
            trade_price,
            fill_qty,
            order->timestamp_ns,
        };
        on_trade_(trade);

        order->remaining_quantity -= fill_qty;
        resting->remaining_quantity -= fill_qty;

        if (resting->remaining_quantity == 0) {
            book_.pop_front(opposite);
        } else {
            book_.reduce_front_quantity(opposite, fill_qty);
        }
    }

    const Quantity filled = original_remaining - order->remaining_quantity;

    if (self_cross_hit) {
        order->remaining_quantity = 0;
        return SubmitResult::SELF_CROSS_REJECTED;
    }

    if (order->remaining_quantity == 0) {
        return SubmitResult::FILLED;
    }

    if (order->type == OrderType::LIMIT) {
        book_.add_order(order);
        return filled > 0 ? SubmitResult::PARTIALLY_RESTED : SubmitResult::RESTED;
    }

    // MARKET or IOC: unfilled remainder is cancelled, never rests.
    order->remaining_quantity = 0;
    return SubmitResult::CANCELLED;
}

bool MatchingEngine::cancel(OrderId order_id) {
    return book_.cancel_order(order_id);
}

}  // namespace matchcore
