#include <iostream>
#include <string>
#include <utility>

#include "server/session.hpp"

namespace engine_server {
    Session::Session(boost::asio::ip::tcp::socket&& socket_) : socket(std::move(socket_)) {
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
            [self](boost::system::error_code ec, std::size_t bytes) {
                if(ec) {
                    return;
                }
                std::istream stream(&self->buffer);
                std::string message;
                std::getline(stream, message);
                std::cout << "Received: " << message << '\n';
                if(!self->handshake_done) {
                    std::string name = "";
                    std::size_t space = message.find(' ');
                    if(space == std::string::npos) {
                        name = "";
                    }
                    else {
                        name = message.substr(space + 1);
                    }
                    self->handshake_done = true;
                    self->trader = name;
                    self->response = "Welcome "  + name + '\n';
                    self->write();
                }
                else{
                    self->response = message + '\n';
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
            [self](boost::system::error_code ec, std::size_t bytes) {
                if(ec) {
                    return;
                }
                self->read();
            }
        );
    }

}