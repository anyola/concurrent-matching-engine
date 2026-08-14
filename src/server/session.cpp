#include <iostream>
#include <string>
#include <utility>
#include <exception>
#include <nlohmann/json.hpp>

#include "server/session.hpp"
#include "server/protocol.hpp"

namespace engine_server {
    Session::Session(boost::asio::ip::tcp::socket&& socket_, matching_engine::Exchange& exchange_, const std::string& remote_address_, const std::string& local_address_) : 
    socket(std::move(socket_)), exchange(exchange_), remote_address(remote_address_), local_address(local_address_) {
        handshake_done = false;
    }
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
                            nlohmann::json response;
                            response["type"] = "welcome";
                            response["trader"] = self->trader;
                            self->response = response.dump() + '\n';
                            self->protocol = std::make_unique<Protocol>(self->exchange, self->trader);
                            self->write();
                        }
                        else {
                            nlohmann::json response;
                            response["type"] = "error";
                            response["message"] = "handshake required";
                            self->response = response.dump() + '\n';
                            self->write();
                        }
                    }
                    else{
                        nlohmann::json request = nlohmann::json::parse(message);
                        nlohmann::json response = self->protocol->process(request);
                        self->response = response.dump() + '\n';
                        self->write();
                    }
                }
                catch(const std::exception& e) {
                    nlohmann::json response;
                    response["type"] = "error";
                    response["message"] = e.what();
                    self->response = response.dump() + '\n';
                    self->write();
                }
                
            }
        );
    }
    void Session::write() {
        auto self = shared_from_this();
        boost::asio::async_write(
            socket,
            boost::asio::buffer(response),
            [self](boost::system::error_code ec, std::size_t) {
                if(ec) {
                    std::cout << "Disconnected " << self->remote_address << " -> " << self->local_address << " (" << ec.message() << ")\n";
                    return;
                }
                self->read();
            }
        );
    }

}