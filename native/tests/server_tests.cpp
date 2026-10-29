// Transport tests: HTTP/1.1 + WebSocket listener with its limits, deadlines and caps.
#include <chrono>
#include <string>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "net_client.hpp"
#include "test_harness.hpp"
#include "transport/handler.hpp"

using namespace manus;
using namespace std::chrono_literals;
namespace websocket = boost::beast::websocket;

namespace {

class EchoHandler final : public transport::MessageHandler {
public:
    std::optional<std::string> on_message(std::string_view raw) override {
        if (raw == "silent") return std::nullopt;
        return "echo:" + std::string(raw);
    }
};

class TestHandler final : public transport::RequestHandler {
public:
    http::HttpReply handle_http(const http::HttpRequest& request) override {
        const std::string target(request.target);
        if (target == "/health") return http::json_reply(200, R"({"ok":true})");
        if (target == "/api/pair" || target == "/echo") {
            return http::text_reply(200, std::string(request.method) + ":" + std::to_string(request.body.size()));
        }
        return http::text_reply(404, "not found");
    }
    std::unique_ptr<transport::MessageHandler> open_websocket(std::string_view target) override {
        if (target.find("token=good") == std::string_view::npos) return nullptr;
        return std::make_unique<EchoHandler>();
    }
};

struct Fixture {
    explicit Fixture(transport::ServerLimits limits = {}) : server(handler, logger, limits) {}
    TestHandler handler;
    log::NullLogger logger;
    test::ServerThread server;
};

std::string get(const std::string& target, const std::string& extra = "") {
    return "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n" + extra + "Connection: close\r\n\r\n";
}

std::string post(const std::string& target, const std::string& body) {
    return "POST " + target + " HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

std::string upgrade(const std::string& target, const std::string& method = "GET",
                    const std::string& version = "13") {
    return method + " " + target +
           " HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
           "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: " +
           version + "\r\n\r\n";
}

using WsStream = websocket::stream<boost::asio::ip::tcp::socket>;

std::unique_ptr<WsStream> ws_connect(boost::asio::io_context& io, unsigned short port, const std::string& target) {
    auto ws = std::make_unique<WsStream>(io);
    ws->next_layer().connect({boost::asio::ip::address_v4::loopback(), port});
    ws->handshake("localhost", target);
    return ws;
}

}  // namespace

TEST_CASE(http_get_returns_handler_reply) {
    Fixture f;
    const auto r = test::exchange_one(f.server.port(), get("/health"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
    CHECK_EQ(test::header_of(*r, "content-type"), std::string("application/json; charset=utf-8"));
    CHECK_EQ(r->body, std::string(R"({"ok":true})"));
    CHECK(r->headers.count("server") == 0);
}

TEST_CASE(http_keep_alive_serves_pipelined_requests) {
    Fixture f;
    const std::string two = "GET /health HTTP/1.1\r\nHost: x\r\n\r\n" + get("/nope");
    const auto responses = test::parse_responses(test::raw_exchange(f.server.port(), two));
    REQUIRE(responses.size() == 2);
    CHECK_EQ(responses[0].status, 200);
    CHECK_EQ(responses[1].status, 404);
    CHECK_EQ(responses[1].body, std::string("not found"));
    CHECK(responses[1].headers.count("content-type") == 0);
}

TEST_CASE(http_header_limit_exact_and_over) {
    Fixture f;
    // A header block a little under 16 KiB is accepted; over 16 KiB is 431.
    const std::string small(15 * 1024, 'a');
    auto r = test::exchange_one(f.server.port(), get("/health", "X-Pad: " + small + "\r\n"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
    const std::string big(17 * 1024, 'a');
    r = test::exchange_one(f.server.port(), get("/health", "X-Pad: " + big + "\r\n"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 431);
}

TEST_CASE(http_body_limit_exact_and_plus_one) {
    Fixture f;
    auto r = test::exchange_one(f.server.port(), post("/api/pair", std::string(4096, 'x')));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
    CHECK_EQ(r->body, std::string("POST:4096"));

    r = test::exchange_one(f.server.port(), post("/api/pair", std::string(4097, 'x')));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
    CHECK_EQ(r->body, std::string(R"({"error":"bad_request"})"));

    r = test::exchange_one(f.server.port(), post("/echo", std::string(4097, 'x')));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 413);
}

TEST_CASE(http_huge_declared_body_is_refused_before_reading) {
    Fixture f;
    const std::string req =
        "POST /api/pair HTTP/1.1\r\nHost: x\r\nContent-Length: 10000000000\r\nConnection: close\r\n\r\n{";
    const auto r = test::exchange_one(f.server.port(), req);
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
}

TEST_CASE(http_chunked_body_over_limit) {
    Fixture f;
    std::string req = "POST /api/pair HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
    req += "1001\r\n" + std::string(4097, 'x') + "\r\n0\r\n\r\n";
    const auto r = test::exchange_one(f.server.port(), req);
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
}

TEST_CASE(http_malformed_request_is_400) {
    Fixture f;
    const auto r = test::exchange_one(f.server.port(), "GARBAGE\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
}

TEST_CASE(http_expect_100_continue) {
    Fixture f;
    const std::string req =
        "POST /echo HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\nContent-Length: 2\r\nConnection: close\r\n\r\nab";
    const auto responses = test::parse_responses(test::raw_exchange(f.server.port(), req));
    REQUIRE(responses.size() == 2);
    CHECK_EQ(responses[0].status, 100);
    CHECK_EQ(responses[1].status, 200);
    CHECK_EQ(responses[1].body, std::string("POST:2"));
}

TEST_CASE(http_keep_alive_idle_timeout_closes) {
    transport::ServerLimits limits;
    limits.keep_alive_timeout = 1s;
    Fixture f(limits);
    const auto start = std::chrono::steady_clock::now();
    const std::string out = test::raw_exchange(f.server.port(), "GET /health HTTP/1.1\r\nHost: x\r\n\r\n", 5000ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK_EQ(test::parse_responses(out).size(), std::size_t{1});
    CHECK(elapsed < 4500ms);  // the server closed the idle connection
}

TEST_CASE(upgrade_to_other_path_is_destroyed_without_response) {
    Fixture f;
    CHECK(test::raw_exchange(f.server.port(), upgrade("/other")).empty());
    CHECK(test::raw_exchange(f.server.port(), upgrade("/health")).empty());
}

TEST_CASE(upgrade_validation_matches_ws_library) {
    Fixture f;
    auto r = test::exchange_one(f.server.port(), upgrade("/ws?token=good", "POST"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 405);
    r = test::exchange_one(f.server.port(), upgrade("/ws?token=good", "GET", "12"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
    CHECK_EQ(test::header_of(*r, "sec-websocket-version"), std::string("13"));
    const std::string bad_key =
        "GET /ws HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
        "Sec-WebSocket-Key: short\r\nSec-WebSocket-Version: 13\r\n\r\n";
    r = test::exchange_one(f.server.port(), bad_key);
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 400);
}

TEST_CASE(websocket_handshake_does_not_advertise_the_library) {
    Fixture f;
    const std::string out = test::raw_exchange(f.server.port(), upgrade("/ws?token=good"), 1000ms);
    CHECK(out.rfind("HTTP/1.1 101", 0) == 0);
    CHECK(out.find("Boost") == std::string::npos);
    CHECK(out.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
}

TEST_CASE(websocket_unauthorized_closes_with_4401) {
    Fixture f;
    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), "/ws?token=bad");
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;
    ws->read(buffer, ec);
    CHECK(ec == websocket::error::closed);
    CHECK_EQ(ws->reason().code, std::uint16_t{4401});
    CHECK_EQ(std::string(ws->reason().reason.c_str()), std::string("unauthorized"));
}

TEST_CASE(websocket_messages_and_replies) {
    Fixture f;
    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), "/ws?token=good");
    ws->write(boost::asio::buffer(std::string("silent")));  // no reply
    ws->write(boost::asio::buffer(std::string("hi")));
    boost::beast::flat_buffer buffer;
    ws->read(buffer);
    CHECK_EQ(boost::beast::buffers_to_string(buffer.data()), std::string("echo:hi"));
    CHECK(ws->got_text());
    // binary frames are read as text too (Node: data.toString())
    ws->binary(true);
    ws->write(boost::asio::buffer(std::string("bin")));
    buffer.consume(buffer.size());
    ws->read(buffer);
    CHECK_EQ(boost::beast::buffers_to_string(buffer.data()), std::string("echo:bin"));
    ws->close(websocket::close_code::normal);
}

TEST_CASE(websocket_message_limit_exact_and_plus_one) {
    Fixture f;
    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), "/ws?token=good");
    const std::string exact(64 * 1024, 'a');  // exactly the limit is accepted
    ws->write(boost::asio::buffer(exact));
    boost::beast::flat_buffer buffer;
    ws->read(buffer);
    CHECK_EQ(buffer.size(), exact.size() + 5);

    const std::string over(64 * 1024 + 1, 'a');
    ws->write(boost::asio::buffer(over));
    buffer.consume(buffer.size());
    boost::system::error_code ec;
    ws->read(buffer, ec);
    CHECK(ec == websocket::error::closed);
    CHECK_EQ(ws->reason().code, std::uint16_t{1009});
}

TEST_CASE(connection_cap_refuses_extra_connections) {
    transport::ServerLimits limits;
    limits.max_connections = 2;
    Fixture f(limits);
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket a(io);
    boost::asio::ip::tcp::socket b(io);
    a.connect({boost::asio::ip::address_v4::loopback(), f.server.port()});
    b.connect({boost::asio::ip::address_v4::loopback(), f.server.port()});
    std::this_thread::sleep_for(200ms);  // let the server register both
    CHECK(test::raw_exchange(f.server.port(), get("/health"), 2000ms).empty());
    a.close();
    b.close();
    std::this_thread::sleep_for(200ms);
    const auto r = test::exchange_one(f.server.port(), get("/health"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
}

TEST_CASE(websocket_cap_answers_503) {
    transport::ServerLimits limits;
    limits.max_websockets = 1;
    Fixture f(limits);
    boost::asio::io_context io;
    auto first = ws_connect(io, f.server.port(), "/ws?token=good");
    const auto r = test::exchange_one(f.server.port(), upgrade("/ws?token=good"));
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 503);
    first->close(websocket::close_code::normal);
}

TEST_CASE(stop_terminates_open_websockets) {
    TestHandler handler;
    log::NullLogger logger;
    auto server = std::make_unique<test::ServerThread>(handler, logger);
    boost::asio::io_context io;
    auto ws = ws_connect(io, server->port(), "/ws?token=good");
    server.reset();  // stop() + join: must not hang on the open websocket
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;
    ws->read(buffer, ec);
    CHECK(static_cast<bool>(ec));
}
