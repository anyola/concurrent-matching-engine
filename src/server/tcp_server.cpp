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
                try{
                    std::string remote_address = socket.remote_endpoint().address().to_string()
                    + ":" + std::to_string(socket.remote_endpoint().port());

                    std::string local_address = socket.local_endpoint().address().to_string()
                    + ":" + std::to_string(socket.local_endpoint().port());

                    std::cout << "Client connected " << remote_address << " -> " << local_address << '\n';

                    auto session = std::make_shared<Session>(std::move(socket), exchange, remote_address, local_address);
                    session->start();
                }
                catch(const std::exception& e) {
                    std::cerr << "Connection failed: " << e.what() << '\n';
                }
               
            }
            accept();
        });
    }

    void TcpServer::start() {
        accept();
    }
}