#include <iostream>
#include <string>
#include <utility>

#include "server/session.hpp"

namespace engine_server {
    Session::Session(boost::asio::ip::tcp::socket&& socket_) : socket(std::move(socket_)) {}
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
                std::istream stream(&buffer);
                std::string message;
                std::getline(stream, message);
                std::cout << "Received:" << message << '\n';
                self->read();
            }
        );
    }

}