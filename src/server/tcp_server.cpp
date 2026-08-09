#include "server/tcp_server.hpp"

namespace engine_server {
    TcpServer::TcpServer(boost::asio::io_context& io_context, unsigned short port) : acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {}
    void TcpServer::accept() {
        acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if(!ec) {
                std::cout << "Client connected\n";
            }
            accept();
        });
    }

    void TcpServer::start() {
        accept();
    }
}