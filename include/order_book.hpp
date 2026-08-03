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

namespace matching_engine {
    class order_error : public std::runtime_error {
    public:
        explicit order_error(const std::string& message) : std::runtime_error(message) {}
    };

    class invalid_price_error : public order_error {
    public:
        explicit invalid_price_error(const std::string& message) : order_error(message) {}
    };
    class invalid_quantity_error : public order_error {
    public:
        explicit invalid_quantity_error(const std::string& message) : order_error(message) {}
    };

    class self_trade_error : public order_error {
    public:
        explicit self_trade_error(const std::string& message) : order_error(message) {}
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

    class OrderBook {
    private:
        std::map<int, std::list<Order>, std::greater<int>> buy;
        std::map<int, std::list<Order>> sell;
        mutable std::mutex mtx;
        std::unordered_map<int, OrderLocation> order_idx; 
    public:

        std::vector<Trade> place_order(const Order& order) {
            std::unique_lock<std::mutex> lock(mtx);
            std::vector<Trade> result;
            if(order.side == Side::BUY) {
                buy[order.price].push_back(order);
                auto it = std::prev(buy[order.price].end());
                order_idx[order.id] = OrderLocation{order.side, order.price, it};
            }
            else{
                sell[order.price].push_back(order);
                auto it = std::prev(sell[order.price].end());
                order_idx[order.id] = OrderLocation{order.side, order.price, it};
            }
            
            return result;
        }
        bool cancel_order(const Order& order) {
            std::unique_lock<std::mutex> lock(mtx);
            int order_id = order.id;
            if(order_idx.find(order_id) != order_idx.end()) {
                auto order_it = order_idx[order_id].it;
                if(order.side == Side::BUY){
                    buy[order.price].erase(order_it);
                }
                else {
                    sell[order.price].erase(order_it);
                }
                order_idx.erase(order_id);
                return true;
            }
            return false;
        }
        Depth snapshot_depth(std::size_t levels) {
            std::unique_lock<std::mutex> lock(mtx);
            Depth result;
                
            for(std::pair<const int, std::list<Order>>& buy_ : buy) {
                DepthLevel dl;
                dl.quantity = 0;
                dl.price = buy_.first;
                for(Order& ord : buy_.second) {
                    dl.quantity += ord.quantity;
                }
                result.bids.push_back(dl);
                if(result.bids.size() == levels){
                    break;
                }
            }
            for(std::pair<const int, std::list<Order>>& sell_ : sell) {
                DepthLevel dl;
                dl.quantity = 0;
                dl.price = sell_.first;
                for(Order& ord : sell_.second) {
                    dl.quantity += ord.quantity;
                }
                result.asks.push_back(dl);
                if(result.asks.size() == levels){
                    break;
                }
            }
            
            return result;
        }
    };
}




#endif