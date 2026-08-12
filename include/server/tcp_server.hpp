#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>
#include "order_book.hpp"

namespace engine_server {
    class TcpServer {
    public:
        TcpServer(boost::asio::io_context& io_context, unsigned short port, matching_engine::OrderBook& order_book_);

        void start();
    private:
        matching_engine::OrderBook& order_book;
        boost::asio::ip::tcp::acceptor acceptor;
        void accept();
    };
}

#endif
