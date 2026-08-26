#include <catch2/catch_test_macros.hpp>
#include <deque>

#include "OrderBook.hpp"

using namespace matchcore;

namespace {

// std::deque never invalidates references/pointers to existing elements on
// push_back, so orders created through this helper stay valid for the life
// of the test even as more are appended — exactly what OrderBook needs
// since it only stores Order* pointers.
class OrderStore {
public:
    Order* make(OrderId id, uint64_t owner, Side side, OrderType type, Price price,
                Quantity qty) {
        orders_.push_back(Order{id, owner, id /* timestamp = id, ascending */, side, type, price,
                                 qty, qty});
        return &orders_.back();
    }

private:
    std::deque<Order> orders_;
};

}  // namespace

TEST_CASE("empty book reports no best price on either side", "[orderbook][phase1]") {
    OrderBook book;
    REQUIRE_FALSE(book.best_price(Side::BUY).has_value());
    REQUIRE_FALSE(book.best_price(Side::SELL).has_value());
    REQUIRE(book.empty(Side::BUY));
    REQUIRE(book.empty(Side::SELL));
}

TEST_CASE("insert maintains price ordering: best bid is highest, best ask is lowest",
          "[orderbook][phase1]") {
    OrderBook book;
    OrderStore store;

    book.add_order(store.make(1, 1, Side::BUY, OrderType::LIMIT, 990000, 10));
    book.add_order(store.make(2, 1, Side::BUY, OrderType::LIMIT, 1005000, 10));
    book.add_order(store.make(3, 1, Side::BUY, OrderType::LIMIT, 980000, 10));
    REQUIRE(book.best_price(Side::BUY) == 1005000);

    book.add_order(store.make(4, 2, Side::SELL, OrderType::LIMIT, 1010000, 10));
    book.add_order(store.make(5, 2, Side::SELL, OrderType::LIMIT, 1000000, 10));
    book.add_order(store.make(6, 2, Side::SELL, OrderType::LIMIT, 1020000, 10));
    REQUIRE(book.best_price(Side::SELL) == 1000000);
}

TEST_CASE("FIFO within a price level: first order in is first order out", "[orderbook][phase1]") {
    OrderBook book;
    OrderStore store;

    Order* first = store.make(1, 1, Side::BUY, OrderType::LIMIT, 1000000, 10);
    Order* second = store.make(2, 1, Side::BUY, OrderType::LIMIT, 1000000, 20);
    Order* third = store.make(3, 1, Side::BUY, OrderType::LIMIT, 1000000, 30);
    book.add_order(first);
    book.add_order(second);
    book.add_order(third);

    REQUIRE(book.peek_front(Side::BUY)->order_id == 1);
    book.pop_front(Side::BUY);
    REQUIRE(book.peek_front(Side::BUY)->order_id == 2);
    book.pop_front(Side::BUY);
    REQUIRE(book.peek_front(Side::BUY)->order_id == 3);
    book.pop_front(Side::BUY);
    REQUIRE(book.empty(Side::BUY));
}

TEST_CASE("cancel removes the correct order and leaves others intact", "[orderbook][phase1]") {
    OrderBook book;
    OrderStore store;

    book.add_order(store.make(1, 1, Side::BUY, OrderType::LIMIT, 1000000, 10));
    book.add_order(store.make(2, 1, Side::BUY, OrderType::LIMIT, 1000000, 20));
    book.add_order(store.make(3, 1, Side::BUY, OrderType::LIMIT, 1000000, 30));

    REQUIRE(book.cancel_order(2));
    REQUIRE(book.peek_front(Side::BUY)->order_id == 1);
    book.pop_front(Side::BUY);
    REQUIRE(book.peek_front(Side::BUY)->order_id == 3);  // order 2 was skipped, not just hidden

    // Cancelling an id that no longer exists (already cancelled/popped) fails cleanly.
    REQUIRE_FALSE(book.cancel_order(2));
}

TEST_CASE("cancelling the last order at a level removes the level entirely",
          "[orderbook][phase1]") {
    OrderBook book;
    OrderStore store;

    book.add_order(store.make(1, 1, Side::SELL, OrderType::LIMIT, 1000000, 10));
    book.add_order(store.make(2, 1, Side::SELL, OrderType::LIMIT, 1010000, 10));

    REQUIRE(book.best_price(Side::SELL) == 1000000);
    REQUIRE(book.cancel_order(1));
    REQUIRE(book.best_price(Side::SELL) == 1010000);  // level 1000000 is gone, not just empty
}
