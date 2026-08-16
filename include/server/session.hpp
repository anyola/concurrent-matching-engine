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
        Session(boost::asio::ip::tcp::socket&& socket_, matching_engine::Exchange& exchange_, const std::string& remote_address_, const std::string& local_address_);

        void start();
    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::streambuf buffer;
        std::string response;
        void read();
        void write(bool continue_read = true);
        bool handshake_done;
        std::string trader;
        matching_engine::Exchange& exchange;
        std::unique_ptr<Protocol> protocol;
        std::string remote_address;
        std::string local_address;
        void start_subscription(const nlohmann::json& request);
    };
}

#endif
