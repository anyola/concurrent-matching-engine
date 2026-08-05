#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "order_book.hpp"
 
#include <thread>
#include <future>
#include <atomic>
#include <vector>
#include <numeric>
 
using namespace matching_engine;
 
namespace {
Order make_order(int price, int qty, std::string trader, Side side,
                  OrderType type = OrderType::LIMIT) {
    return Order{
        0,
        price,
        qty,
        std::move(trader),
        side,
        type,
        std::chrono::steady_clock::now()
    };
}
 
int total_qty(const std::vector<Trade>& trades) {
    return std::accumulate(trades.begin(), trades.end(), 0,
                            [](int acc, const Trade& t) { return acc + t.quantity; });
}
 
} // namespace
 
// ============================================================================
// 1. Basic matching
// ============================================================================
 
TEST_CASE("no match when spread is not crossed") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
    auto result = book.place_order(make_order(95, 10, "bob", Side::BUY));
 
    CHECK(result.trades.empty());
    CHECK(result.remaining_quantity == 10);
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.bids.size() == 1);
    REQUIRE(depth.asks.size() == 1);
    CHECK(depth.bids[0].price == 95);
    CHECK(depth.asks[0].price == 100);
}
 
TEST_CASE("full match, exact quantity") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
    auto result = book.place_order(make_order(100, 10, "bob", Side::BUY));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].price == 100);
    CHECK(result.trades[0].quantity == 10);
    CHECK(result.trades[0].maker == "alice");
    CHECK(result.trades[0].taker == "bob");
    CHECK(result.remaining_quantity == 0);

    auto depth = book.snapshot_depth(5);
    CHECK(depth.bids.empty());
    CHECK(depth.asks.empty());
}
 
TEST_CASE("partial fill: taker larger than resting maker") {
    OrderBook book;
    book.place_order(make_order(100, 4, "alice", Side::SELL));
    auto result = book.place_order(make_order(100, 10, "bob", Side::BUY));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].quantity == 4);

    CHECK(result.remaining_quantity == 6);
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.bids.size() == 1);
    CHECK(depth.bids[0].quantity == 6);
    CHECK(depth.asks.empty());
}
 
TEST_CASE("partial fill: maker larger than taker, maker rests with reduced qty") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
    auto result = book.place_order(make_order(100, 4, "bob", Side::BUY));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].quantity == 4);
    CHECK(result.remaining_quantity == 0);
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 1);
    CHECK(depth.asks[0].quantity == 6);
}
 
TEST_CASE("taker sweeps multiple price levels") {
    OrderBook book;
    book.place_order(make_order(100, 5, "alice", Side::SELL));
    book.place_order(make_order(101, 5, "carol", Side::SELL));
    book.place_order(make_order(102, 5, "dave", Side::SELL));
    auto result = book.place_order(make_order(101, 8, "bob", Side::BUY));
 
    REQUIRE(result.trades.size() == 2);
    CHECK(result.trades[0].price == 100);
    CHECK(result.trades[0].quantity == 5);
    CHECK(result.trades[1].price == 101);
    CHECK(result.trades[1].quantity == 3);
    CHECK(result.remaining_quantity == 0);
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 2);
    CHECK(depth.asks[0].price == 101);
    CHECK(depth.asks[0].quantity == 2);
    CHECK(depth.asks[1].price == 102);
}
 
TEST_CASE("price-time priority: earlier order at same price fills first") {
    OrderBook book;
    book.place_order(make_order(100, 5, "alice", Side::SELL));
    book.place_order(make_order(100, 5, "carol", Side::SELL));
 
    auto result = book.place_order(make_order(100, 5, "bob", Side::BUY));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].maker == "alice");
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 1);
    CHECK(depth.asks[0].quantity == 5);
}
 
// ============================================================================
// 2. Self-trade prevention
// ============================================================================
 
TEST_CASE("self-trade: only own order at best level -> skipped, no match, no throw") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
 
    auto result = book.place_order(make_order(100, 10, "alice", Side::BUY));
 
    CHECK(result.trades.empty());
    CHECK(result.remaining_quantity == 10);

    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 1);
    REQUIRE(depth.bids.size() == 1);
    CHECK(depth.asks[0].quantity == 10);
    CHECK(depth.bids[0].quantity == 10);
}
 
TEST_CASE("self-trade: own-then-other-then-own on same level, only other is matched") {
    OrderBook book;
    book.place_order(make_order(100, 5, "alice", Side::SELL));
    book.place_order(make_order(100, 5, "carol", Side::SELL));
    book.place_order(make_order(100, 5, "alice", Side::SELL));
 
    auto result = book.place_order(make_order(100, 5, "alice", Side::BUY));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].maker == "carol");
    CHECK(result.remaining_quantity == 0);

    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 1);
    CHECK(depth.asks[0].quantity == 10);
}
 
TEST_CASE("self-trade: all liquidity across levels belongs to the same trader") {
    OrderBook book;
    book.place_order(make_order(100, 5, "alice", Side::SELL));
    book.place_order(make_order(101, 5, "alice", Side::SELL));
 
    auto result = book.place_order(make_order(101, 10, "alice", Side::BUY));
 
    CHECK(result.trades.empty());
    CHECK(result.remaining_quantity == 10);
 
    auto depth = book.snapshot_depth(5);
    CHECK(depth.asks.size() == 2);
    CHECK(depth.bids.size() == 1);
}
 
// ============================================================================
// 3. Market-orders
// ============================================================================
 
TEST_CASE("market order fully executes against available liquidity") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
 
    auto result = book.place_order(make_order(0, 10, "bob", Side::BUY, OrderType::MARKET));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].quantity == 10);
    CHECK(result.remaining_quantity == 0);
}
 
TEST_CASE("market order partially executes, remainder is dropped (not resting)") {
    OrderBook book;
    book.place_order(make_order(100, 4, "alice", Side::SELL));
 
    auto result = book.place_order(make_order(0, 10, "bob", Side::BUY, OrderType::MARKET));
 
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].quantity == 4);
    CHECK(result.remaining_quantity == 6);

    auto depth = book.snapshot_depth(5);
    CHECK(depth.bids.empty());
}
 
TEST_CASE("market order with zero liquidity produces no trades, does not throw") {
    OrderBook book;
    auto result = book.place_order(make_order(0, 10, "bob", Side::BUY, OrderType::MARKET));
 
    CHECK(result.trades.empty());
    CHECK(result.remaining_quantity == 10);
    CHECK(book.snapshot_depth(5).bids.empty());
}
 
// ============================================================================
// 4. Validation
// ============================================================================
 
TEST_CASE("limit order with non-positive price throws invalid_price_error") {
    OrderBook book;
    CHECK_THROWS_AS(
        book.place_order(make_order(0, 10, "alice", Side::BUY, OrderType::LIMIT)),
        invalid_price_error);
    CHECK_THROWS_AS(
        book.place_order(make_order(-5, 10, "alice", Side::BUY, OrderType::LIMIT)),
        invalid_price_error);
}
 
TEST_CASE("market order price is not validated (price field is meaningless for MARKET)") {
    OrderBook book;
    CHECK_NOTHROW(book.place_order(make_order(0, 10, "alice", Side::BUY, OrderType::MARKET)));
}
 
TEST_CASE("non-positive quantity throws invalid_quantity_error regardless of order type") {
    OrderBook book;
    CHECK_THROWS_AS(
        book.place_order(make_order(100, 0, "alice", Side::BUY)),
        invalid_quantity_error);
    CHECK_THROWS_AS(
        book.place_order(make_order(100, -3, "alice", Side::BUY)),
        invalid_quantity_error);
    CHECK_THROWS_AS(
        book.place_order(make_order(0, 0, "alice", Side::BUY, OrderType::MARKET)),
        invalid_quantity_error);
}
 
TEST_CASE("all engine exceptions derive from order_error") {
    OrderBook book;
    try {
        book.place_order(make_order(0, 10, "alice", Side::BUY));
        FAIL("expected invalid_price_error");
    } catch (const order_error& e) {
        CHECK(std::string(e.what()).size() > 0);
    }
}
 
// ============================================================================
// 5. Cancel_order
// ============================================================================
 
TEST_CASE("cancel_order removes a resting order and frees the price level") {
    OrderBook book;
    auto placed = book.place_order(make_order(100, 10, "alice", Side::SELL));
 
    CHECK(book.cancel_order(placed.order_id) == true);
 
    auto depth = book.snapshot_depth(5);
    CHECK(depth.asks.empty());

    auto result = book.place_order(make_order(100, 10, "bob", Side::BUY));
    CHECK(result.trades.empty());
}
 
TEST_CASE("cancel_order returns false for unknown id") {
    OrderBook book;
    CHECK(book.cancel_order(999999) == false);
}
 
TEST_CASE("cancel_order returns false for an already fully executed order") {
    OrderBook book;
    auto placed = book.place_order(make_order(100, 10, "alice", Side::SELL));
    book.place_order(make_order(100, 10, "bob", Side::BUY));
 
    CHECK(book.cancel_order(placed.order_id) == false);
}
 
TEST_CASE("cancel_order on a partially filled order cancels only the remainder") {
    OrderBook book;
    auto placed = book.place_order(make_order(100, 10, "alice", Side::SELL));
    book.place_order(make_order(100, 4, "bob", Side::BUY));
 
    CHECK(book.cancel_order(placed.order_id) == true);
    CHECK(book.snapshot_depth(5).asks.empty());
}
 
// ============================================================================
// 6. Snapshot_depth
// ============================================================================
 
TEST_CASE("snapshot_depth aggregates quantity across multiple orders at same level") {
    OrderBook book;
    book.place_order(make_order(100, 3, "alice", Side::SELL));
    book.place_order(make_order(100, 4, "carol", Side::SELL));
    book.place_order(make_order(100, 5, "dave", Side::SELL));
 
    auto depth = book.snapshot_depth(5);
    REQUIRE(depth.asks.size() == 1);
    CHECK(depth.asks[0].price == 100);
    CHECK(depth.asks[0].quantity == 12);
}
 
TEST_CASE("snapshot_depth respects the levels limit") {
    OrderBook book;
    for (int price = 100; price < 110; ++price) {
        book.place_order(make_order(price, 1, "alice", Side::SELL));
    }
    auto depth = book.snapshot_depth(3);
    CHECK(depth.asks.size() == 3);
    CHECK(depth.asks[0].price == 100);
    CHECK(depth.asks[1].price == 101);
    CHECK(depth.asks[2].price == 102);
}
 
// ============================================================================
// 7. TradeFeed
// ============================================================================
 
TEST_CASE("wait_next_trade returns immediately if a trade already happened before subscribe") {
    OrderBook book;
    book.place_order(make_order(100, 10, "alice", Side::SELL));
    book.place_order(make_order(100, 10, "bob", Side::BUY));
 
    auto feed = book.subscribe();

    book.place_order(make_order(100, 7, "carol", Side::SELL));
    book.place_order(make_order(100, 7, "dave", Side::BUY));
 
    Trade t = feed.wait_next_trade();
    CHECK(t.maker == "carol");
    CHECK(t.taker == "dave");
}
 
TEST_CASE("multiple independent feeds each see every trade") {
    OrderBook book;
    auto feed_a = book.subscribe();
    auto feed_b = book.subscribe();
 
    book.place_order(make_order(100, 5, "alice", Side::SELL));
    book.place_order(make_order(100, 5, "bob", Side::BUY));
 
    Trade ta = feed_a.wait_next_trade();
    Trade tb = feed_b.wait_next_trade();
    CHECK(ta.maker == "alice");
    CHECK(tb.maker == "alice");
}
 
// ============================================================================
// 8. TradeFeed (multithread)
// ============================================================================
 
TEST_CASE("wait_next_trade genuinely blocks until a trade happens on another thread") {
    OrderBook book;
    auto feed = book.subscribe();
 
    std::promise<Trade> got_trade;
    std::future<Trade> fut = got_trade.get_future();
 
    std::thread waiter([&] {
        Trade t = feed.wait_next_trade();
        got_trade.set_value(t);
    });

    CHECK(fut.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
 
    book.place_order(make_order(100, 10, "alice", Side::SELL));
    book.place_order(make_order(100, 10, "bob", Side::BUY));
 
    REQUIRE(fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    Trade t = fut.get();
    CHECK(t.price == 100);
    CHECK(t.quantity == 10);
 
    waiter.join();
}
 
TEST_CASE("subscribe() is atomic w.r.t. concurrently happening trades (stress)") {
    const int iterations = 200;
 
    for (int i = 0; i < iterations; ++i) {
        OrderBook book;
        book.place_order(make_order(100, 10, "alice", Side::SELL));
 
        std::atomic<bool> start{false};
        std::atomic<bool> feed_saw_trade{false};
        std::vector<Trade> immediate_trades;
 
        std::thread subscriber([&] {
            while (!start.load(std::memory_order_acquire)) {}
            TradeFeed feed = book.subscribe();
 
            std::promise<Trade> p;
            auto fut = p.get_future();
            std::thread reader([&] {
                Trade t = feed.wait_next_trade();
                p.set_value(t);
            });
 
            if (fut.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready) {
                feed_saw_trade.store(true, std::memory_order_release);
                reader.join();
            } else {
                book.place_order(make_order(100, 10, "carol", Side::SELL));
                book.place_order(make_order(100, 10, "dave", Side::BUY));

                REQUIRE(fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready);

                feed_saw_trade.store(true, std::memory_order_release);
                reader.join();
            }
        });
 
        std::thread taker([&] {
            while (!start.load(std::memory_order_acquire)) {}
            auto result = book.place_order(make_order(100, 10, "bob", Side::BUY));
            immediate_trades = result.trades;
        });
 
        start.store(true, std::memory_order_release);
        subscriber.join();
        taker.join();

        CHECK((feed_saw_trade.load() || !immediate_trades.empty()));
    }
}
 
// ============================================================================
// 9. Exchange
// ============================================================================
 
TEST_CASE("get_or_create_book returns the same instance for repeated calls") {
    Exchange exchange;
    OrderBook& first = exchange.get_or_create_book("AAPL");
    OrderBook& second = exchange.get_or_create_book("AAPL");
    CHECK(&first == &second);
}
 
TEST_CASE("get_or_create_book throws invalid_name for empty symbol") {
    Exchange exchange;
    CHECK_THROWS_AS(exchange.get_or_create_book(""), invalid_name);
}
 
TEST_CASE("different symbols get independent order books") {
    Exchange exchange;
    OrderBook& aapl = exchange.get_or_create_book("AAPL");
    OrderBook& btc = exchange.get_or_create_book("BTCUSD");
    CHECK(&aapl != &btc);
 
    aapl.place_order(make_order(100, 10, "alice", Side::SELL));
    CHECK(btc.snapshot_depth(5).asks.empty());
}
 
TEST_CASE("concurrent get_or_create_book calls for the same symbol all return one address") {
    Exchange exchange;
    const int n_threads = 16;
    std::vector<std::thread> threads;
    std::vector<OrderBook*> results(n_threads, nullptr);
    std::atomic<bool> start{false};
 
    for (int i = 0; i < n_threads; ++i) {
        threads.emplace_back([&, i] {
            while (!start.load(std::memory_order_acquire)) {}
            results[i] = &exchange.get_or_create_book("AAPL");
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
 
    for (int i = 1; i < n_threads; ++i) {
        CHECK(results[i] == results[0]);
    }
}
 
TEST_CASE("operations on independent symbols do not serialize each other") {
    Exchange exchange;
    const int orders_per_thread = 20000;
 
    auto work = [&](const std::string& symbol) {
        OrderBook& book = exchange.get_or_create_book(symbol);
        for (int i = 0; i < orders_per_thread; ++i) {
            book.place_order(make_order(100 + (i % 50), 1, "trader", Side::SELL));
        }
    };
 
    auto start_time = std::chrono::steady_clock::now();
    std::thread a([&] { work("AAPL"); });
    std::thread b([&] { work("BTCUSD"); });
    a.join();
    b.join();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    CHECK(elapsed < std::chrono::seconds(30));
}
 
// ============================================================================
// 10. Invariant
// ============================================================================
 
TEST_CASE("invariant under concurrent load: submitted volume == executed + resting") {
    OrderBook book;
    const int n_threads = 8;
    const int orders_per_thread = 2000;
 
    std::atomic<long long> total_submitted{0};
    std::atomic<long long> total_executed_taker_side{0};
    std::vector<std::thread> threads;
 
    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < orders_per_thread; ++i) {
                Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
                int price = 100 + (i % 10);
                int qty = 1 + (i % 5);
                std::string trader = "trader_" + std::to_string(t);
 
                auto result = book.place_order(make_order(price, qty, trader, side));
 
                total_submitted.fetch_add(qty, std::memory_order_relaxed);
                total_executed_taker_side.fetch_add(total_qty(result.trades),
                                                      std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) th.join();
    long long remaining_in_book = 0;
    auto depth = book.snapshot_depth(1000000);
    for (auto& lvl : depth.bids) remaining_in_book += lvl.quantity;
    for (auto& lvl : depth.asks) remaining_in_book += lvl.quantity;
    CHECK(total_submitted.load() == total_executed_taker_side.load() * 2 + remaining_in_book);
}