#include <stdexcept>
#include "server/protocol.hpp"

namespace engine_server {
    CommandType parse_command_type(const std::string& type) {
        if(type == "place_order") {
            return CommandType::PlaceOrder;
        }
        else if (type == "cancel_order") {
            return CommandType::CancelOrder;
        }
        else if (type == "snapshot_depth") {
            return CommandType::SnapshotDepth;
        }
        else {
            return CommandType::Unknown;
        }
    }

    Protocol::Protocol(matching_engine::Exchange& exchange_, const std::string& trader_) : exchange(exchange_), trader(trader_) {} 
    
    matching_engine::Side parse_side(const std::string& side) {
        if(side == "buy") {
            return matching_engine::Side::BUY;
        }
        else if (side == "sell"){
            return matching_engine::Side::SELL;
        }
        else {
            throw std::runtime_error("invalid side");
        }
    }
    matching_engine::OrderType parse_order_type(const std::string& order_type) {
        if(order_type == "limit") {
            return matching_engine::OrderType::LIMIT;
        }
        else if(order_type == "market"){
            return matching_engine::OrderType::MARKET;
        }
        else {
            throw std::runtime_error("invalid order type");
        }
    }

    nlohmann::json Protocol::process(const nlohmann::json& request) {
        std::string type = request.at("type");
        std::string symbol = request.at("symbol");
        matching_engine::OrderBook& order_book = exchange.get_or_create_book(symbol);
        CommandType command = parse_command_type(type);
        switch(command) {
            case CommandType::PlaceOrder: {
                matching_engine::Order order;
                order.submitted_at = std::chrono::steady_clock::now();
                order.price = request.at("price");
                order.quantity = request.at("quantity");
                order.trader = trader;
                order.side = parse_side(request.at("side"));
                order.type = parse_order_type(request.at("order_type"));
                matching_engine::PlaceResult result  = order_book.place_order(order);
                nlohmann::json response;
                response["type"] = "order_placed";
                response["order_id"] = result.order_id;
                response["trades"] = nlohmann::json::array();
                for(const auto& trade : result.trades) {
                    nlohmann::json trade_json;
                    trade_json["maker"] = trade.maker;
                    trade_json["maker_id"] = trade.maker_id;
                    trade_json["taker"] = trade.taker;
                    trade_json["taker_id"] = trade.taker_id;
                    trade_json["price"] = trade.price;
                    trade_json["quantity"] = trade.quantity;
                    response["trades"].push_back(trade_json);
                }
                response["remaining_quantity"] = result.remaining_quantity;
                return response;
            }

            case CommandType::CancelOrder: {
                int order_id = request.at("order_id");
                bool result = order_book.cancel_order(order_id);
                nlohmann::json response;
                response["type"] = "order_cancelled";
                response["order_id"] = order_id;
                response["success"]  = result;
                return response;
            }

            case CommandType::SnapshotDepth: {
                int levels = request.at("levels");
                matching_engine::Depth result = order_book.snapshot_depth(levels);
                nlohmann::json response;
                response["type"] = "snapshot_depth";
                response["bids"] = nlohmann::json::array();
                response["asks"] = nlohmann::json::array();
                for(const auto& bid : result.bids) {
                    nlohmann::json bid_json;
                    bid_json["price"] = bid.price;
                    bid_json["quantity"] = bid.quantity;
                    response["bids"].push_back(bid_json);
                }
                for(const auto& ask : result.asks) {
                    nlohmann::json ask_json;
                    ask_json["price"] = ask.price;
                    ask_json["quantity"] = ask.quantity;
                    response["asks"].push_back(ask_json);
                }
                return response;
            } 
            case CommandType::Unknown: {
                nlohmann::json response;
                response["type"] = "error";
                response["message"] = "unknown command";
                return response;
            }
        }  
        throw std::runtime_error("unreachable"); 
    }


}