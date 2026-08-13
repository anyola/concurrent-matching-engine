#include <boost/asio.hpp>

#include "server/tcp_server.hpp"

int main() {
    boost::asio::io_context io;
    matching_engine::Exchange exchange;
    engine_server::TcpServer server(io, 8080, exchange);
    server.start();
    io.run();
    return 0;
}