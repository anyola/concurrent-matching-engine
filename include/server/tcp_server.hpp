#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>
#include "order_book.hpp"

namespace engine_server {
    class TcpServer {
    public:
        TcpServer(boost::asio::io_context& io_context, unsigned short port, matching_engine::Exchange& exchange_);

        void start();
    private:
        boost::asio::ip::tcp::acceptor acceptor;
        matching_engine::Exchange& exchange;
        void accept();
    };
}

#endif
