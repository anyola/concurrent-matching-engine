#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
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
    matching_engine::Side parse_side(const std::string& side);
    matching_engine::OrderType parse_order_type(const std::string& order_type);

    class Protocol {
    public:
        Protocol(matching_engine::Exchange& exchange_, const std::string& trader_);
        nlohmann::json process(const nlohmann::json& request);
    private:
        matching_engine::Exchange& exchange;
        std::string trader;
    };
}

#endif