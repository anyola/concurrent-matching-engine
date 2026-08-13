#include <iostream>
#include <memory>
#include <utility>

#include "server/tcp_server.hpp"
#include "server/session.hpp"

namespace engine_server {
    TcpServer::TcpServer(boost::asio::io_context& io_context, unsigned short port, matching_engine::Exchange& exchange_) : acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), exchange(exchange_) {}
    void TcpServer::accept() {
        acceptor.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if(!ec) {
                std::cout << "Client connected\n";
                auto session = std::make_shared<Session>(std::move(socket), exchange);
                session->start();
            }
            accept();
        });
    }

    void TcpServer::start() {
        accept();
    }
}