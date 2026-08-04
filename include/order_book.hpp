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
#include <algorithm>
#include <atomic>
#include <queue>
#include <condition_variable>

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
    class invalid_name : public order_error {
    public:
        explicit invalid_name(const std::string& message) : order_error(message) {}
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

        int generate_id(){
            return next_id.fetch_add(1);
        }

        std::vector<Trade> match(Order& order) {
            std::vector<Trade> trades;
            if(order.side == Side::BUY) {
                for(auto level_it = sell.begin(); level_it != sell.end();) {
                    if(order.type == OrderType::MARKET || level_it->first <= order.price) {
                        for(auto order_it = level_it->second.begin(); order_it != level_it->second.end();) {
                            if(order.quantity == 0) {
                                break;
                            }
                            if(order_it->trader == order.trader) {
                                order_it++;
                                continue;
                            }
                            else{
                                int trade_qty = std::min(order_it->quantity, order.quantity);
                                int trade_price = level_it->first;
                                Trade trade = {order_it->trader, order_it->id, order.trader, order.id, trade_price, trade_qty, std::chrono::system_clock::now()};
                                trades.push_back(trade);
                                trade_log.push_back(trade);
                                trade_cv.notify_all();
                                order.quantity -= trade_qty;
                                order_it->quantity -= trade_qty;
                                if(order_it->quantity == 0) {
                                    int maker_id = order_it->id;
                                    order_idx.erase(maker_id);
                                    order_it = level_it->second.erase(order_it);
                                }
                                else{
                                    order_it++;
                                }
                            }
                        }
                        if(order.quantity == 0) {
                            break;
                        }
                        if(level_it->second.empty()) {
                            level_it =sell.erase(level_it);    
                        }
                        else{
                            level_it++;
                        }
                    }
                    else {
                        break;
                    }  
                }
            }
            else {
                for(auto level_it = buy.begin(); level_it != buy.end();) {
                    if(order.type == OrderType::MARKET || level_it->first >= order.price) {
                        for(auto order_it = level_it->second.begin(); order_it != level_it->second.end();) {
                            if(order.quantity == 0) {
                                break;
                            }
                            if(order_it->trader == order.trader) {
                                order_it++;
                                continue;
                            }
                            else{
                                int trade_qty = std::min(order_it->quantity, order.quantity);
                                int trade_price = level_it->first;
                                Trade trade = {order_it->trader, order_it->id, order.trader, order.id, trade_price, trade_qty, std::chrono::system_clock::now()};
                                trades.push_back(trade);
                                trade_log.push_back(trade);
                                trade_cv.notify_all();
                                order.quantity -= trade_qty;
                                order_it->quantity -= trade_qty;
                                if(order_it->quantity == 0) {
                                    int maker_id = order_it->id;
                                    order_idx.erase(maker_id);
                                    order_it = level_it->second.erase(order_it);
                                }
                                else{
                                    order_it++;
                                }
                            }
                        }
                        if(order.quantity == 0) {
                            break;
                        }
                        if(level_it->second.empty()) {
                            level_it = buy.erase(level_it);    
                        }
                        else{
                            level_it++;
                        }
                    }
                    else {
                        break;
                    }
                }   
            }
            return trades;
        }

    public:
        std::vector<Trade> place_order(Order& order) {
            std::unique_lock<std::mutex> lock(mtx);
            std::vector<Trade> result;
            if(order.type == OrderType::LIMIT && order.price <= 0) {
                throw invalid_price_error("invalid price");
            }
            if(order.quantity <= 0) {
                throw invalid_quantity_error("invalid quantity");
            }
            order.id = generate_id();
            if(order.side == Side::BUY) {
                result = match(order);
                if(order.quantity > 0 && order.type == OrderType::LIMIT) {
                    buy[order.price].push_back(order);
                    auto it = std::prev(buy[order.price].end());
                    order_idx[order.id] = OrderLocation{order.side, order.price, it};
                }
            }
            else{
                result = match(order);
                if(order.quantity > 0 && order.type == OrderType::LIMIT) {
                    sell[order.price].push_back(order);
                    auto it = std::prev(sell[order.price].end());
                    order_idx[order.id] = OrderLocation{order.side, order.price, it};
                }   
            }
            return result;
        }

        bool cancel_order(const Order& order) {
            std::unique_lock<std::mutex> lock(mtx);
            auto idx = order_idx.find(order.id);
            
            if(idx != order_idx.end()) {
                OrderLocation location = idx->second;
                auto order_it = location.it;
                if(location.side == Side::BUY){
                    buy[location.price].erase(order_it);
                    if(buy[location.price].empty()){
                        buy.erase(location.price);
                    }
                }
                else {
                    sell[location.price].erase(order_it);
                    if(sell[location.price].empty()){
                        sell.erase(location.price);
                    }
                }
                order_idx.erase(idx);
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
                for(const Order& ord : buy_.second) {
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
                for(const Order& ord : sell_.second) {
                    dl.quantity += ord.quantity;
                }
                result.asks.push_back(dl);
                if(result.asks.size() == levels){
                    break;
                }
            }
            
            return result;
        }
        TradeFeed subscribe() {
            std::unique_lock<std::mutex> lock(mtx);
            TradeFeed result = {*this, trade_log.size()};
            return result;
        }
    };

    class TradeFeed {
        OrderBook& book;
        std::size_t next_trade;
    public:
        TradeFeed(OrderBook& b, std::size_t nt) : book(b), next_trade(nt) {}
        Trade wait_next_trade() {
            std::unique_lock<std::mutex> lock(book.mtx);
            book.trade_cv.wait(lock, [this]{return next_trade < book.trade_log.size();});
            Trade result = book.trade_log[next_trade];
            next_trade++;
            return result;
        }
    };

    class Exchange {
        std::mutex book_mutex;
        std::unordered_map<std::string, std::unique_ptr<OrderBook>> book_map;
    public:
        OrderBook& get_or_create_book(const std::string& symbol) {
            std::unique_lock<std::mutex> lock(book_mutex);
            if(!symbol.empty()){
                auto it = book_map.find(symbol);
                if(it != book_map.end()) {
                    return *(it->second.get());
                }
                else{
                    book_map[symbol] = std::make_unique<OrderBook>();
                    return *book_map[symbol].get();
                }
            }
            else {
                throw invalid_name("empty name");
            }
        }
    };

}

#endif