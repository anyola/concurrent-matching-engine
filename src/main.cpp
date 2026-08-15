#include <boost/asio.hpp>
#include <iostream>
#include <limits>
#include <string>
#include <fstream>

#include "server/tcp_server.hpp"

int main(int argc, char* argv[]) {
    boost::asio::io_context io;
    matching_engine::Exchange exchange;
    if(argc != 3) {
        std::cerr << "invalid number of arguments" << '\n';
        return 1;
    }
    std::string port_str = argv[1];
    unsigned short port;
    try {
        std::size_t pos = 0;
        int port_num = std::stoi(port_str, &pos);
        if(pos != port_str.size() || port_num < 0 || port_num > std::numeric_limits<unsigned short>::max()) {
            std::cerr << "invalid port: " << port_str << '\n';
            return 1;
        }
        port = static_cast<unsigned short>(port_num);
    }
    catch(const std::exception& e) {
        std::cerr << "invalid port: " << argv[1] << '\n';
        return 1;
    }
    engine_server::TcpServer server(io, port, exchange);
    unsigned short received_port = server.port();
    std::cout << "listening at " << received_port << '\n';
    
    std::ofstream port_file(argv[2]);
    if(!port_file) {
        std::cerr << "unable to store port in file " << argv[2] << '\n';
        return 1;
    }
    port_file << received_port << '\n';
    server.start();
    io.run();
    return 0;
}