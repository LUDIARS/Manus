// End to end inside one process: ManusApp + transport + Pairing + FakeInputSink
// (server.ts wiring). Uses 127.0.0.1 with an ephemeral port; no real input is injected.
#include <chrono>
#include <string>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

#include "app/manus_app.hpp"
#include "auth/pairing.hpp"
#include "fake_input_sink.hpp"
#include "net_client.hpp"
#include "temp_dir.hpp"
#include "test_harness.hpp"

using namespace manus;
namespace websocket = boost::beast::websocket;

namespace {

constexpr const char* kSalt = "00112233445566778899aabbccddeeff";

struct AppFixture {
    AppFixture()
        : pairing(auth::StoredPin{kSalt, auth::hash_pin("123456", kSalt)}, std::string(32, '\x07'), 24,
                  [this] { return now; }),
          app(sink, pairing, logger, public_dir.path()),
          server(app, logger) {}

    std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    test::FakeInputSink sink;
    log::NullLogger logger;
    test::TempDir public_dir;
    auth::Pairing pairing;
    app::ManusApp app;
    test::ServerThread server;
};

std::string pair_request(const std::string& pin) {
    const std::string body = R"({"pin":")" + pin + R"("})";
    return "POST /api/pair HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

std::string get_token(AppFixture& f) {
    const auto r = test::exchange_one(f.server.port(), pair_request("123456"));
    if (!r || r->status != 200) return {};
    return nlohmann::json::parse(r->body)["token"].get<std::string>();
}

using WsStream = websocket::stream<boost::asio::ip::tcp::socket>;

std::unique_ptr<WsStream> ws_connect(boost::asio::io_context& io, unsigned short port, const std::string& target) {
    auto ws = std::make_unique<WsStream>(io);
    ws->next_layer().connect({boost::asio::ip::address_v4::loopback(), port});
    ws->handshake("localhost", target);
    return ws;
}

std::uint16_t close_code_for(AppFixture& f, const std::string& target) {
    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), target);
    boost::beast::flat_buffer buffer;
    boost::system::error_code ec;
    ws->read(buffer, ec);
    return ec == websocket::error::closed ? ws->reason().code : 0;
}

}  // namespace

TEST_CASE(health_reflects_the_input_sink) {
    AppFixture f;
    auto r = test::exchange_one(f.server.port(), "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->body, std::string(R"({"ok":true,"service":"manus","helper":"up"})"));
    f.sink.up = false;
    r = test::exchange_one(f.server.port(), "GET /health?x HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->body, std::string(R"({"ok":true,"service":"manus","helper":"down"})"));
}

TEST_CASE(pair_then_websocket_injects_and_acks) {
    AppFixture f;
    const auto bad = test::exchange_one(f.server.port(), pair_request("000000"));
    REQUIRE(bad.has_value());
    CHECK_EQ(bad->status, 401);
    const std::string token = get_token(f);
    REQUIRE(!token.empty());

    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), "/ws?token=" + token);
    ws->text(true);
    ws->write(boost::asio::buffer(std::string(R"({"type":"key","key":"c","modifiers":["ctrl"],"seq":1})")));
    boost::beast::flat_buffer buffer;
    ws->read(buffer);
    CHECK_EQ(boost::beast::buffers_to_string(buffer.data()), std::string(R"({"type":"ack","seq":1})"));
    ws->write(boost::asio::buffer(std::string(R"({"type":"ping","seq":2})")));
    buffer.consume(buffer.size());
    ws->read(buffer);
    CHECK_EQ(boost::beast::buffers_to_string(buffer.data()), std::string(R"({"type":"pong","seq":2})"));
    ws->write(boost::asio::buffer(std::string("not json")));
    buffer.consume(buffer.size());
    ws->read(buffer);
    CHECK(boost::beast::buffers_to_string(buffer.data()).find(R"("code":"bad_message")") != std::string::npos);
    ws->close(websocket::close_code::normal);
    REQUIRE(f.sink.calls.size() == 1);
    CHECK_EQ(f.sink.calls[0].size(), std::size_t{4});
}

TEST_CASE(websocket_token_checks_close_with_4401) {
    AppFixture f;
    const std::string token = get_token(f);
    REQUIRE(!token.empty());
    std::string tampered = token;
    tampered.back() = tampered.back() == '0' ? '1' : '0';
    CHECK_EQ(close_code_for(f, "/ws"), std::uint16_t{4401});
    CHECK_EQ(close_code_for(f, "/ws?token="), std::uint16_t{4401});
    CHECK_EQ(close_code_for(f, "/ws?token=garbage"), std::uint16_t{4401});
    CHECK_EQ(close_code_for(f, "/ws?token=" + tampered), std::uint16_t{4401});
    f.now += 24LL * 3'600'000;  // the token's expiry
    CHECK_EQ(close_code_for(f, "/ws?token=" + token), std::uint16_t{4401});
    CHECK(f.sink.calls.empty());
}

TEST_CASE(token_query_is_url_decoded) {
    AppFixture f;
    const std::string token = get_token(f);
    REQUIRE(!token.empty());
    // the browser sends encodeURIComponent(token); "." may also arrive as %2E
    std::string encoded = token;
    encoded.replace(encoded.find('.'), 1, "%2E");
    boost::asio::io_context io;
    auto ws = ws_connect(io, f.server.port(), "/ws?x=1&token=" + encoded);
    ws->write(boost::asio::buffer(std::string(R"({"type":"ping","seq":1})")));
    boost::beast::flat_buffer buffer;
    ws->read(buffer);
    CHECK_EQ(boost::beast::buffers_to_string(buffer.data()), std::string(R"({"type":"pong","seq":1})"));
    ws->close(websocket::close_code::normal);
}

TEST_CASE(pair_lockout_over_http) {
    AppFixture f;
    for (int i = 0; i < 5; ++i) CHECK_EQ(test::exchange_one(f.server.port(), pair_request("999999"))->status, 401);
    CHECK_EQ(test::exchange_one(f.server.port(), pair_request("123456"))->status, 429);
    f.now += 60'000;
    CHECK_EQ(test::exchange_one(f.server.port(), pair_request("123456"))->status, 200);
}

TEST_CASE(static_files_over_http) {
    AppFixture f;
    f.public_dir.write("index.html", "<!doctype html><title>Manus</title>");
    f.public_dir.write("js/main.js", "export {};");
    const std::string get_root = "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    auto r = test::exchange_one(f.server.port(), get_root);
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
    CHECK_EQ(r->body, std::string("<!doctype html><title>Manus</title>"));
    CHECK_EQ(test::header_of(*r, "content-type"), std::string("text/html; charset=utf-8"));
    CHECK_EQ(test::header_of(*r, "cache-control"), std::string("no-store, no-cache, must-revalidate"));
    const std::string etag = test::header_of(*r, "etag");

    r = test::exchange_one(f.server.port(), "GET / HTTP/1.1\r\nHost: x\r\nIf-None-Match: " + etag +
                                                "\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 304);
    CHECK(r->body.empty());

    // Any method falls back to static files, as in Node (POST / and GET /api/pair serve index.html).
    r = test::exchange_one(f.server.port(), "POST /keyboard HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);
    r = test::exchange_one(f.server.port(), "GET /api/pair HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200);

    r = test::exchange_one(f.server.port(), "GET /js/main.js HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->body, std::string("export {};"));
    CHECK_EQ(test::header_of(*r, "content-type"), std::string("text/javascript; charset=utf-8"));

    r = test::exchange_one(f.server.port(), "GET /..%2f..%2fetc/hosts.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 403);
    r = test::exchange_one(f.server.port(), "GET /nope.css HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 404);
}

TEST_CASE(head_sends_headers_only) {
    AppFixture f;
    f.public_dir.write("index.html", "0123456789");
    const std::string out =
        test::raw_exchange(f.server.port(), "HEAD / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(out.rfind("HTTP/1.1 200", 0) == 0);
    CHECK(out.find("Content-Length: 10\r\n") != std::string::npos);
    CHECK(out.size() == out.find("\r\n\r\n") + 4);  // no body bytes
}
