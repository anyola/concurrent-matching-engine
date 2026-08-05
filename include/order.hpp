#ifndef ORDER_HPP
#define ORDER_HPP

#include <chrono>
#include <list>
#include <string>
#include <vector>

namespace matching_engine {

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

}

#endif