#ifndef SESSION_HPP
#define SESSION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include "server/protocol.hpp"

namespace matching_engine {
    class OrderBook;
}
namespace engine_server {
    class Protocol;

    class Session : public std::enable_shared_from_this<Session>{
    public:
        Session(boost::asio::ip::tcp::socket&& socket_, matching_engine::OrderBook& order_book_);

        void start();
    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::streambuf buffer;
        std::string response;
        void read();
        void write();
        bool handshake_done;
        std::string trader;
        matching_engine::OrderBook& order_book;
        std::unique_ptr<Protocol> protocol;
    };
}

#endif
