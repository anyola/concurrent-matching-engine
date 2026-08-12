#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <nlohmann/json.hpp>
#include "order_book.hpp"

namespace engine_server {
    enum class CommandType {
        PlaceOrder,
        CancelOrder,
        SnapshotDepth,
        Unknown
    };

    CommandType  parse_command_type(const std::string& type);
    Side parse_side(const std::string& side);
    OrderType parse_order_type(const std::string& order_type);

    class Protocol {
    public:
        Protocol(OrderBook& order_book_, const std::string& trader_);
        nlohmann::json process(const nlohmann::json& request);
    private:
        OrderBook& order_book;
        std::string trader;
    };
}

#endif