#include <boost/asio.hpp>

#include "server/tcp_server.hpp"

int main() {
    boost::asio::io_context io;
    TcpServer server(io, 8080);
    server.start();
    io.run();
    return 0;
}