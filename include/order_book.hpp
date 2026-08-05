#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP
 
#include <chrono>
#include <stdexcept>
#include <string>
#include <map>
#include <list>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
 
namespace matching_engine {
    class order_error : public std::runtime_error {
    public:
        explicit order_error(const std::string& message);
    };
 
    class invalid_price_error : public order_error {
    public:
        explicit invalid_price_error(const std::string& message);
    };
    class invalid_quantity_error : public order_error {
    public:
        explicit invalid_quantity_error(const std::string& message);
    };
    class invalid_name : public order_error {
    public:
        explicit invalid_name(const std::string& message);
    };
 
    enum class Side {
        BUY,
        SELL
    };
 
    enum class OrderType {
        LIMIT,
        MARKET
    };
 
    struct Order {
        int id;
        int price;
        int quantity;
        std::string trader;
        Side side;
        OrderType type;
        std::chrono::steady_clock::time_point submitted_at;
    };
 
    struct Trade {
        std::string maker;
        int maker_id;
        std::string taker;
        int taker_id;
        int price;
        int quantity;
        std::chrono::system_clock::time_point executed_at;
    };
 
    struct DepthLevel {
        int price;
        int quantity;
    };
    struct Depth {
        std::vector<DepthLevel> bids;
        std::vector<DepthLevel> asks;
    };
 
    struct OrderLocation {
        Side side;
        int price;
        std::list<Order>::iterator it;
    };
 
    struct PlaceResult {
        int order_id;
        std::vector<Trade> trades;
        int remaining_quantity;
    };
 
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