#ifndef SESSION_HPP
#define SESSION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace engine_server {
    class Session : public std::enable_shared_from_this<Session>{
    public:
        Session(boost::asio::ip::tcp::socket&& socket_);

        void start();
    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::streambuf buffer;
        std::string response;
        void read();
        void write();
        bool handshake_done;
        std::string trader;
    };
}

#endif
