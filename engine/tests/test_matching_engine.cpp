#include <catch2/catch_test_macros.hpp>
#include <deque>
#include <vector>

#include "MatchingEngine.hpp"

using namespace matchcore;

namespace {

class OrderStore {
public:
    Order* make(OrderId id, uint64_t owner, Side side, OrderType type, Price price,
                Quantity qty) {
        orders_.push_back(Order{id, owner, id, side, type, price, qty, qty});
        return &orders_.back();
    }

private:
    std::deque<Order> orders_;
};

}  // namespace

TEST_CASE("full fill: matching quantities at a crossing price fully consume both orders",
          "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    Order* resting_sell = store.make(1, 100, Side::SELL, OrderType::LIMIT, 1000000, 10);
    REQUIRE(engine.submit(resting_sell) == SubmitResult::RESTED);

    Order* buy = store.make(2, 200, Side::BUY, OrderType::LIMIT, 1000000, 10);
    SubmitResult result = engine.submit(buy);

    REQUIRE(result == SubmitResult::FILLED);
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].price == 1000000);
    REQUIRE(trades[0].quantity == 10);
    REQUIRE(trades[0].buy_order_id == 2);
    REQUIRE(trades[0].sell_order_id == 1);
    REQUIRE(engine.book().empty(Side::SELL));
    REQUIRE(engine.book().empty(Side::BUY));
}

TEST_CASE("partial fill: unmatched remainder rests on the book", "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    engine.submit(store.make(1, 100, Side::SELL, OrderType::LIMIT, 1000000, 10));

    Order* buy = store.make(2, 200, Side::BUY, OrderType::LIMIT, 1000000, 15);
    SubmitResult result = engine.submit(buy);

    REQUIRE(result == SubmitResult::PARTIALLY_RESTED);
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 10);
    REQUIRE(buy->remaining_quantity == 5);
    REQUIRE(engine.book().peek_front(Side::BUY)->remaining_quantity == 5);
}

TEST_CASE("multi-level sweep: aggressive order walks multiple price levels in price order",
          "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    engine.submit(store.make(1, 100, Side::SELL, OrderType::LIMIT, 1000000, 5));
    engine.submit(store.make(2, 100, Side::SELL, OrderType::LIMIT, 1010000, 5));
    engine.submit(store.make(3, 100, Side::SELL, OrderType::LIMIT, 1020000, 5));

    Order* buy = store.make(4, 200, Side::BUY, OrderType::LIMIT, 1020000, 17);
    SubmitResult result = engine.submit(buy);

    REQUIRE(result == SubmitResult::PARTIALLY_RESTED);
    REQUIRE(trades.size() == 3);
    REQUIRE(trades[0].price == 1000000);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(trades[1].price == 1010000);
    REQUIRE(trades[1].quantity == 5);
    REQUIRE(trades[2].price == 1020000);
    REQUIRE(trades[2].quantity == 5);
    REQUIRE(buy->remaining_quantity == 2);
    REQUIRE(engine.book().best_price(Side::BUY) == 1020000);
}

TEST_CASE("IOC: unfilled remainder is cancelled, never rests", "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    engine.submit(store.make(1, 100, Side::SELL, OrderType::LIMIT, 1000000, 5));

    Order* buy_ioc = store.make(2, 200, Side::BUY, OrderType::IOC, 1000000, 10);
    SubmitResult result = engine.submit(buy_ioc);

    REQUIRE(result == SubmitResult::CANCELLED);
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(buy_ioc->remaining_quantity == 0);
    REQUIRE(engine.book().empty(Side::BUY));  // remainder did not rest
}

TEST_CASE("market order: sweeps regardless of price and never rests", "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    engine.submit(store.make(1, 100, Side::SELL, OrderType::LIMIT, 1000000, 5));
    engine.submit(store.make(2, 100, Side::SELL, OrderType::LIMIT, 1010000, 5));

    Order* buy_market = store.make(3, 200, Side::BUY, OrderType::MARKET, 0, 8);
    SubmitResult result = engine.submit(buy_market);

    REQUIRE(result == SubmitResult::FILLED);
    REQUIRE(trades.size() == 2);
    REQUIRE(trades[0].price == 1000000);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(trades[1].price == 1010000);
    REQUIRE(trades[1].quantity == 3);
    REQUIRE(engine.book().peek_front(Side::SELL)->remaining_quantity == 2);
}

TEST_CASE("self-cross policy: aggressor is rejected before trading against its own resting order",
          "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    constexpr uint64_t self_owner = 42;
    constexpr uint64_t other_owner = 99;

    engine.submit(store.make(1, self_owner, Side::BUY, OrderType::LIMIT, 1000000, 10));

    Order* sell = store.make(2, self_owner, Side::SELL, OrderType::LIMIT, 1000000, 10);
    SubmitResult result = engine.submit(sell);

    REQUIRE(result == SubmitResult::SELF_CROSS_REJECTED);
    REQUIRE(trades.empty());
    REQUIRE(sell->remaining_quantity == 0);         // rejected, not resting
    REQUIRE(engine.book().empty(Side::SELL));
    REQUIRE(engine.book().peek_front(Side::BUY)->remaining_quantity == 10);  // untouched

    (void)other_owner;
}

TEST_CASE("self-cross policy: fills against other owners stand before the self-cross is hit",
          "[matching][phase2]") {
    OrderStore store;
    std::vector<Trade> trades;
    MatchingEngine engine([&trades](const Trade& t) { trades.push_back(t); });

    constexpr uint64_t self_owner = 42;
    constexpr uint64_t other_owner = 99;

    engine.submit(store.make(1, other_owner, Side::SELL, OrderType::LIMIT, 990000, 5));
    engine.submit(store.make(2, self_owner, Side::SELL, OrderType::LIMIT, 1000000, 10));

    Order* buy = store.make(3, self_owner, Side::BUY, OrderType::LIMIT, 1000000, 12);
    SubmitResult result = engine.submit(buy);

    REQUIRE(result == SubmitResult::SELF_CROSS_REJECTED);
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].price == 990000);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(buy->remaining_quantity == 0);  // 7 leftover cancelled, not resting
    REQUIRE(engine.book().peek_front(Side::SELL)->order_id == 2);  // self order untouched
    REQUIRE(engine.book().peek_front(Side::SELL)->remaining_quantity == 10);
}

TEST_CASE("cancel: resting order can be cancelled through the engine", "[matching][phase2]") {
    OrderStore store;
    MatchingEngine engine([](const Trade&) {});

    engine.submit(store.make(1, 100, Side::BUY, OrderType::LIMIT, 1000000, 10));
    REQUIRE(engine.cancel(1));
    REQUIRE(engine.book().empty(Side::BUY));
    REQUIRE_FALSE(engine.cancel(1));  // already gone
}
