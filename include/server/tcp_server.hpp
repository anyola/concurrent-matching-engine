#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>

namespace engine_server {
    class TcpServer {
    public:
        TcpServer(boost::asio::io_context& io_context, unsigned short port);

        void start();
    private:
        boost::asio::ip::tcp::acceptor acceptor;
        void accept();
    };
}

#endif
