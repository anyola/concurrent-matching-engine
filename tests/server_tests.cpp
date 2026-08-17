#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "server/tcp_server.hpp"
#include "order_book.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

using boost::asio::ip::tcp;

namespace {

struct ServerFixture {
    matching_engine::Exchange exchange;
    boost::asio::io_context io;
    engine_server::TcpServer server;
    std::thread io_thread;
    unsigned short port;

    ServerFixture() : server(io, 0, exchange) {
        port = server.port();
        server.start();
        io_thread = std::thread([this] { io.run(); });
    }

    ~ServerFixture() {
        io.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
};

ServerFixture& fixture() {
    static ServerFixture instance;
    return instance;
}

struct Client {
    tcp::iostream stream;

    explicit Client(unsigned short port) {
        stream.connect("127.0.0.1", std::to_string(port));
        REQUIRE(stream.good());
    }

    nlohmann::json send(const nlohmann::json& request) {
        stream << request.dump() << "\n";
        stream.flush();
        std::string line;
        std::getline(stream, line);
        REQUIRE(!line.empty());
        return nlohmann::json::parse(line);
    }

    nlohmann::json hello(const std::string& trader) {
        return send({{"type", "hello"}, {"trader", trader}});
    }
};

} // namespace

// ============================================================================
// 1. Handshake
// ============================================================================

TEST_CASE("handshake: hello returns welcome with the same trader name") {
    Client c(fixture().port);
    auto resp = c.hello("alice_handshake");
    CHECK(resp["type"] == "welcome");
    CHECK(resp["trader"] == "alice_handshake");
}

TEST_CASE("handshake: non-hello first message returns an error, not a silent drop") {
    Client c(fixture().port);
    auto resp = c.send({{"type", "snapshot_depth"}, {"symbol", "X"}, {"levels", 5}});
    CHECK(resp["type"] == "error");
}

// ============================================================================
// 2. Place_order
// ============================================================================

TEST_CASE("place_order: resting sell + crossing buy produces a trade over the wire") {
    const std::string symbol = "NET_MATCH";

    Client seller(fixture().port);
    seller.hello("seller_net");
    auto sell_resp = seller.send({
        {"type", "place_order"}, {"symbol", symbol}, {"side", "sell"},
        {"order_type", "limit"}, {"price", 100}, {"quantity", 10}
    });
    CHECK(sell_resp["type"] == "order_placed");
    CHECK(sell_resp["trades"].empty());

    Client buyer(fixture().port);
    buyer.hello("buyer_net");
    auto buy_resp = buyer.send({
        {"type", "place_order"}, {"symbol", symbol}, {"side", "buy"},
        {"order_type", "limit"}, {"price", 100}, {"quantity", 10}
    });
    CHECK(buy_resp["type"] == "order_placed");
    REQUIRE(buy_resp["trades"].size() == 1);
    CHECK(buy_resp["trades"][0]["price"] == 100);
    CHECK(buy_resp["trades"][0]["quantity"] == 10);
    CHECK(buy_resp["trades"][0]["maker"] == "seller_net");
    CHECK(buy_resp["trades"][0]["taker"] == "buyer_net");
    CHECK(buy_resp["remaining_quantity"] == 0);
}

TEST_CASE("place_order: invalid price surfaces the engine's exception message") {
    const std::string symbol = "NET_INVALID";
    Client c(fixture().port);
    c.hello("trader_invalid");
    auto resp = c.send({
        {"type", "place_order"}, {"symbol", symbol}, {"side", "buy"},
        {"order_type", "limit"}, {"price", 0}, {"quantity", 10}
    });
    CHECK(resp["type"] == "error");
    CHECK(resp["message"].get<std::string>().size() > 0);
}

// ============================================================================
// 3. Cancel_order
// ============================================================================

TEST_CASE("cancel_order: successful cancel, then repeated cancel returns false") {
    const std::string symbol = "NET_CANCEL";
    Client c(fixture().port);
    c.hello("canceller");
    auto placed = c.send({
        {"type", "place_order"}, {"symbol", symbol}, {"side", "sell"},
        {"order_type", "limit"}, {"price", 200}, {"quantity", 5}
    });
    int order_id = placed["order_id"];
    auto cancel1 = c.send({{"type", "cancel_order"}, {"symbol", symbol}, {"order_id", order_id}});
    CHECK(cancel1["type"] == "order_cancelled");
    CHECK(cancel1["success"] == true);

    auto cancel2 = c.send({{"type", "cancel_order"}, {"symbol", symbol}, {"order_id", order_id}});
    CHECK(cancel2["success"] == false);
}

// ============================================================================
// 4. Snapshot_depth
// ============================================================================

TEST_CASE("snapshot_depth: reflects resting orders and rejects non-positive levels") {
    const std::string symbol = "NET_DEPTH";
    Client c(fixture().port);
    c.hello("depth_trader");
    c.send({{"type", "place_order"}, {"symbol", symbol}, {"side", "sell"},
             {"order_type", "limit"}, {"price", 150}, {"quantity", 7}});

    auto depth = c.send({{"type", "snapshot_depth"}, {"symbol", symbol}, {"levels", 5}});
    CHECK(depth["type"] == "snapshot_depth");
    REQUIRE(depth["asks"].size() == 1);
    CHECK(depth["asks"][0]["price"] == 150);
    CHECK(depth["asks"][0]["quantity"] == 7);

    auto bad = c.send({{"type", "snapshot_depth"}, {"symbol", symbol}, {"levels", 0}});
    CHECK(bad["type"] == "error");
}

// ============================================================================
// 5. Symbol independence
// ============================================================================

TEST_CASE("different symbols do not leak liquidity into each other over the network") {
    Client c(fixture().port);
    c.hello("isolation_trader");
    c.send({{"type", "place_order"}, {"symbol", "NET_ISO_A"}, {"side", "sell"},
             {"order_type", "limit"}, {"price", 100}, {"quantity", 10}});

    auto depth_b = c.send({{"type", "snapshot_depth"}, {"symbol", "NET_ISO_B"}, {"levels", 5}});
    CHECK(depth_b["asks"].empty());
    CHECK(depth_b["bids"].empty());
}

// ============================================================================
// 6. Incorrect input
// ============================================================================

TEST_CASE("regression: malformed JSON returns an error and the server survives") {
    const std::string symbol = "NET_MALFORMED";
    {
        Client c(fixture().port);
        c.hello("malformed_trader");
        c.stream << R"({"type":"place_order","symbol":")" << symbol << "\n";
        c.stream.flush();
        std::string line;
        std::getline(c.stream, line);
        REQUIRE(!line.empty());
        auto resp = nlohmann::json::parse(line);
        CHECK(resp["type"] == "error");
    }
    
    Client c2(fixture().port);
    auto resp = c2.hello("still_alive_after_malformed");
    CHECK(resp["type"] == "welcome");
}

TEST_CASE("regression: place_order missing a required field does not crash the process") {
    const std::string symbol = "NET_MISSING_FIELD";
    {
        Client c(fixture().port);
        c.hello("missing_field_trader");
        auto resp = c.send({
            {"type", "place_order"}, {"symbol", symbol}, {"side", "buy"},
            {"order_type", "limit"}, {"quantity", 10}
        });
        CHECK(resp["type"] == "error");
    }
    
    Client c2(fixture().port);
    auto resp = c2.hello("still_alive_after_missing_field");
    CHECK(resp["type"] == "welcome");
}

TEST_CASE("regression: unknown side/order_type values return an error, not a crash") {
    const std::string symbol = "NET_BAD_ENUM";
    Client c(fixture().port);
    c.hello("bad_enum_trader");
    auto resp = c.send({
        {"type", "place_order"}, {"symbol", symbol}, {"side", "sideways"},
        {"order_type", "limit"}, {"price", 100}, {"quantity", 10}
    });
    CHECK(resp["type"] == "error");

    Client c2(fixture().port);
    auto resp2 = c2.hello("still_alive_after_bad_enum");
    CHECK(resp2["type"] == "welcome");
}

// ============================================================================
// 7. Subscribe
// ============================================================================

TEST_CASE("subscribe: acknowledges subscription and delivers a live trade") {
    const std::string symbol = "NET_SUBSCRIBE";

    Client subscriber(fixture().port);
    subscriber.hello("subscriber_live");
    auto sub_ack = subscriber.send({{"type", "subscribe"}, {"symbol", symbol}});
    CHECK(sub_ack["type"] == "subscribed");
    CHECK(sub_ack["symbol"] == symbol);

    std::promise<std::string> trade_line_promise;
    auto trade_line_future = trade_line_promise.get_future();
    std::thread reader([&subscriber, &trade_line_promise] {
        std::string line;
        if (std::getline(subscriber.stream, line)) {
            trade_line_promise.set_value(line);
        }
    });

    REQUIRE(trade_line_future.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);

    Client seller(fixture().port);
    seller.hello("seller_sub");
    seller.send({{"type", "place_order"}, {"symbol", symbol}, {"side", "sell"},
                  {"order_type", "limit"}, {"price", 100}, {"quantity", 10}});

    Client buyer(fixture().port);
    buyer.hello("buyer_sub");
    buyer.send({{"type", "place_order"}, {"symbol", symbol}, {"side", "buy"},
                 {"order_type", "limit"}, {"price", 100}, {"quantity", 10}});

    REQUIRE(trade_line_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto trade_json = nlohmann::json::parse(trade_line_future.get());
    CHECK(trade_json["type"] == "trade");
    CHECK(trade_json["symbol"] == symbol);
    CHECK(trade_json["maker"] == "seller_sub");
    CHECK(trade_json["taker"] == "buyer_sub");
    CHECK(trade_json["quantity"] == 10);

    reader.join();
}

// ============================================================================
// 8. Burst of trades
// ============================================================================

TEST_CASE("regression: a burst of trades under backpressure is not corrupted") {
    const std::string symbol = "NET_BURST";
    const int n = 500;

    boost::asio::io_context client_io;
    tcp::socket sub_socket(client_io);
    sub_socket.open(tcp::v4());
    sub_socket.set_option(boost::asio::socket_base::receive_buffer_size(2048));
    sub_socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), fixture().port));

    auto send_line = [](tcp::socket& s, const nlohmann::json& j) {
        std::string line = j.dump() + "\n";
        boost::asio::write(s, boost::asio::buffer(line));
    };
    auto read_line = [](tcp::socket& s, boost::asio::streambuf& buf) {
        boost::asio::read_until(s, buf, '\n');
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        return line;
    };

    boost::asio::streambuf sub_buf;
    send_line(sub_socket, {{"type", "hello"}, {"trader", "burst_subscriber"}});
    read_line(sub_socket, sub_buf);
    send_line(sub_socket, {{"type", "subscribe"}, {"symbol", symbol}});
    read_line(sub_socket, sub_buf);

    Client maker(fixture().port);
    maker.hello("burst_maker");
    for (int i = 0; i < n; ++i) {
        maker.send({{"type", "place_order"}, {"symbol", symbol}, {"side", "sell"},
                     {"order_type", "limit"}, {"price", 100}, {"quantity", 1}});
    }

    Client taker(fixture().port);
    taker.hello("burst_taker");
    auto taker_resp = taker.send({{"type", "place_order"}, {"symbol", symbol}, {"side", "buy"},
                                    {"order_type", "limit"}, {"price", 100}, {"quantity", n}});
    CHECK(taker_resp["remaining_quantity"] == 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    sub_socket.non_blocking(true);
    boost::system::error_code ec;
    std::string all_data;
    char chunk[65536];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        std::size_t bytes = sub_socket.read_some(boost::asio::buffer(chunk), ec);
        if (ec == boost::asio::error::would_block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (ec) break; 
        all_data.append(chunk, bytes);
    }
    sub_socket.close(ec);

    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        auto pos = all_data.find('\n', start);
        if (pos == std::string::npos) break;
        std::string line = all_data.substr(start, pos - start);
        if (!line.empty()) lines.push_back(line);
        start = pos + 1;
    }

    INFO("received ", lines.size(), " lines, expected ", n);
    REQUIRE(lines.size() == static_cast<std::size_t>(n));


    int valid_trades = 0;
    for (const auto& line : lines) {
        nlohmann::json parsed;
        bool parse_ok = true;
        try {
            parsed = nlohmann::json::parse(line);
        } catch (const std::exception&) {
            parse_ok = false;
        }
        REQUIRE_MESSAGE(parse_ok, "corrupted line under backpressure: ", line);
        if (parse_ok && parsed.value("type", "") == "trade") {
            ++valid_trades;
        }
    }
    CHECK(valid_trades == n);
}

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();
    std::_Exit(res);
}