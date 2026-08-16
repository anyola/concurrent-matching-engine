#include <iostream>
#include <string>
#include <utility>
#include <exception>
#include <nlohmann/json.hpp>

#include "server/session.hpp"
#include "server/protocol.hpp"

namespace engine_server {
    Session::Session(boost::asio::ip::tcp::socket&& socket_, matching_engine::Exchange& exchange_, const std::string& remote_address_, const std::string& local_address_) : 
    socket(std::move(socket_)), exchange(exchange_), remote_address(remote_address_), local_address(local_address_), handshake_done(false) {}
    void Session::start() {
        read();
    }
    void Session::read() {
        auto self = shared_from_this();
        boost::asio::async_read_until(
            socket,
            buffer,
            '\n',
            [self](boost::system::error_code ec, std::size_t) {
                if(ec) {
                    std::cout << "Disconnected " << self->remote_address << " -> " << self->local_address << " (" << ec.message() << ")\n";
                    return;
                }
                std::istream stream(&self->buffer);
                std::string message;
                std::getline(stream, message);
                std::cout << "Received: " << message << '\n';
                try{
                    if(!self->handshake_done) {
                        nlohmann::json request = nlohmann::json::parse(message);
                        std::string type =  request.at("type");
                        std::string trader = request.at("trader");
                        if(type == "hello") {
                            self->handshake_done = true;
                            self->trader = trader;
                            nlohmann::json response_json;
                            response_json["type"] = "welcome";
                            response_json["trader"] = self->trader;
                            self->response = response_json.dump() + '\n';
                            self->protocol = std::make_unique<Protocol>(self->exchange, self->trader);
                            self->write();
                        }
                        else {
                            nlohmann::json response_json;
                            response_json["type"] = "error";
                            response_json["message"] = "handshake required";
                            self->response = response_json.dump() + '\n';
                            self->write();
                        }
                    }
                    else{
                        nlohmann::json request = nlohmann::json::parse(message);
                        if(request.at("type") == "subscribe") {
                            self->start_subscription(request);
                        }
                        else{
                            nlohmann::json response_json = self->protocol->process(request);
                            self->response = response_json.dump() + '\n';
                            self->write();
                        }
                        
                    }
                }
                catch(const std::exception& e) {
                    nlohmann::json response_json;
                    response_json["type"] = "error";
                    response_json["message"] = e.what();
                    self->response = response_json.dump() + '\n';
                    self->write();
                }
                
            }
        );
    }

    void Session::start_subscription(const nlohmann::json& request) {
        auto self = shared_from_this();

        std::string symbol = request.at("symbol");

        matching_engine::OrderBook& order_book =
            exchange.get_or_create_book(symbol);

        matching_engine::TradeFeed feed = order_book.subscribe();

        nlohmann::json response_json;
        response_json["type"] = "subscribed";
        response_json["symbol"] = symbol;

        self->response = response_json.dump() + '\n';
        self->write(false);

        std::thread([self, symbol, feed = std::move(feed)]() mutable {
            while(true) {
                matching_engine::Trade trade = feed.wait_next_trade();

                boost::asio::post(
                    self->socket.get_executor(),
                    [self, symbol, trade]() {
                        nlohmann::json response_json;
                        response_json["type"] = "trade";
                        response_json["symbol"] = symbol;
                        response_json["maker"] = trade.maker;
                        response_json["maker_id"] = trade.maker_id;
                        response_json["taker"] = trade.taker;
                        response_json["taker_id"] = trade.taker_id;
                        response_json["price"] = trade.price;
                        response_json["quantity"] = trade.quantity;

                        self->response = response_json.dump() + '\n';
                        self->write(false);
                    }
                );
            }
        }).detach();
    }

    void Session::write(bool continue_read) {
        auto self = shared_from_this();
        boost::asio::async_write(
            socket,
            boost::asio::buffer(response),
            [self, continue_read](boost::system::error_code ec, std::size_t) {
                if(ec) {
                    std::cout << "Disconnected " << self->remote_address << " -> " << self->local_address << " (" << ec.message() << ")\n";
                    return;
                }
                if(continue_read) {
                    self->read();
                }   
            }
        );
    }

}