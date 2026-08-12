#include <boost/asio.hpp>

#include "server/tcp_server.hpp"

int main() {
    boost::asio::io_context io;
    matching_engine::OrderBook order_book;
    engine_server::TcpServer server(io, 8080, order_book);
    server.start();
    io.run();
    return 0;
}