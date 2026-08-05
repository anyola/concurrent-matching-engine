#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "errors.hpp"
#include "order.hpp"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace matching_engine {
    class TradeFeed;
    class OrderBook {
    private:
        friend class TradeFeed;
        std::map<int, std::list<Order>, std::greater<int>> buy;
        std::map<int, std::list<Order>> sell;
        mutable std::mutex mtx;
        std::unordered_map<int, OrderLocation> order_idx;
        std::atomic<int> next_id{1};
        std::vector<Trade> trade_log;
        std::condition_variable trade_cv;

        int generate_id();
        std::vector<Trade> match(Order& order);

    public:
        PlaceResult place_order(Order order);
        bool cancel_order(int order_id);
        Depth snapshot_depth(std::size_t levels);
        TradeFeed subscribe();
    };

    class TradeFeed {
        OrderBook& book;
        std::size_t next_trade;
    public:
        TradeFeed(OrderBook& b, std::size_t nt);
        Trade wait_next_trade();
    };

    class Exchange {
        std::mutex book_mutex;
        std::unordered_map<std::string, std::unique_ptr<OrderBook>> book_map;
    public:
        OrderBook& get_or_create_book(const std::string& symbol);
    };

}

#endif