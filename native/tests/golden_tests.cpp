// Compatibility with the Node implementation: replays the golden records made by
// tools/record-golden.ts (fixtures/golden/*.json) against the C++ code.
//
// Compared exactly: VK table, command conversion (the helper command JSON),
// every reply except bad_message's free-text "message", pairing hashes/tokens/
// lock-out, HTTP status/content-type/cache-control/etag presence/body, upgrade
// handling and WebSocket close codes. Intentional differences are listed in
// kSessionDivergences / kHttpDivergences with the reason and the C++ result.
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

#include "app/manus_app.hpp"
#include "auth/pairing.hpp"
#include "auth/pairing_store.hpp"
#include "fake_input_sink.hpp"
#include "inject/jis_vk_map.hpp"
#include "net_client.hpp"
#include "temp_dir.hpp"
#include "test_harness.hpp"
#include "ws/protocol.hpp"
#include "ws/session.hpp"

using namespace manus;
using json = nlohmann::json;
namespace websocket = boost::beast::websocket;

namespace {

const std::filesystem::path kFixtures = MANUS_FIXTURES_DIR;

json load(const std::string& name) {
    std::ifstream in(kFixtures / "golden" / name, std::ios::binary);
    if (!in) throw std::runtime_error("missing golden file " + name + " (run tools/record-golden.ts)");
    return json::parse(in);
}

// bad_message carries zod's / JSON.parse's free text: compare everything but that.
bool same_reply(const std::string& cpp, const json& node) {
    if (node.is_null()) return cpp == "<none>";
    const std::string expected = node.get<std::string>();
    if (cpp == expected) return true;
    const json e = json::parse(expected);
    if (e.value("code", "") != "bad_message" || !json::accept(cpp)) return false;
    const json c = json::parse(cpp);
    return c.value("type", "") == "error" && c.value("code", "") == "bad_message" && !c.contains("seq") &&
           c.contains("message") && c["message"].is_string();
}

struct Divergence {
    const char* reason;
    const char* cpp_reply;    // "<none>" for no reply, "bad_message" for any bad_message
    const char* cpp_command;  // nullptr when no command is produced
};

// raw message -> what C++ does instead of the recorded Node behaviour.
const std::map<std::string, Divergence> kSessionDivergences = {
    {R"({"type":"key","key":"constructor","seq":27})",
     {"Node looked the name up on a plain object and found Object.prototype.constructor (vks [null])",
      R"({"type":"error","code":"inject_failed","message":"unknown key name: constructor","seq":27})", nullptr}},
    {R"({"type":"key","key":"__proto__"})",
     {"Node found Object.prototype (vks [{}]); the C# helper then failed without a reply (no seq)",
      R"({"type":"error","code":"inject_failed","message":"unknown key name: __proto__"})", nullptr}},
    {R"({"type":"text","text":"\ud800","seq":28})",
     {"a lone surrogate is not valid UTF-16 text; nlohmann_json rejects it while parsing", "bad_message", nullptr}},
    {R"({"type":"mouse.move","dx":3000000000,"dy":0,"seq":29})",
     {"the recorder's fake helper accepts everything; the real C# helper failed in Convert.ToInt32 as C++ does",
      R"({"type":"error","code":"inject_failed","message":"Value was either too large or too small for an Int32.","seq":29})",
      R"({"op":"mouse_move","dx":3000000000,"dy":0})"}},
};

const std::map<std::string, std::pair<const char*, int>> kHttpDivergences = {
    {"sibling_prefix",
     {"Node's startsWith(publicRoot) check served ../public-evil/x.js; C++ checks per segment", 403}},
};

std::string command_json_or_null(const std::string& raw) {
    const auto parsed = ws::parse_client_message(raw);
    if (!parsed.message || std::holds_alternative<ws::PingMessage>(*parsed.message)) return "null";
    const auto command = ws::to_command(*parsed.message);
    return command.command ? inject::command_to_json(*command.command) : "null";
}

}  // namespace

TEST_CASE(vk_table_matches_node) {
    const json golden = load("vk_map.json");
    std::map<std::string, int> node;
    for (const auto& [name, vk] : golden["JIS_VK_MAP"].items()) node[name] = vk.get<int>();
    std::map<std::string, int> cpp;
    for (const auto& [name, vk] : inject::jis_vk_table()) cpp[std::string(name)] = vk;
    CHECK(node == cpp);
    CHECK_EQ(node.size(), cpp.size());
    CHECK_EQ(golden["MODIFIER_VK"]["ctrl"].get<int>(), int{inject::modifier_vk(inject::Modifier::ctrl)});
    CHECK_EQ(golden["MODIFIER_VK"]["shift"].get<int>(), int{inject::modifier_vk(inject::Modifier::shift)});
    CHECK_EQ(golden["MODIFIER_VK"]["alt"].get<int>(), int{inject::modifier_vk(inject::Modifier::alt)});
    CHECK_EQ(golden["MODIFIER_VK"]["win"].get<int>(), int{inject::modifier_vk(inject::Modifier::win)});
}

TEST_CASE(session_replies_and_commands_match_node) {
    const json golden = load("session.json");
    int compared = 0;
    for (const auto& c : golden["cases"]) {
        const std::string raw = c["raw"].get<std::string>();
        test::FakeInputSink sink;
        ws::Session session(sink);
        const std::string reply = session.on_message(raw).value_or("<none>");
        const std::string command = command_json_or_null(raw);
        const std::string node_command = c["command"].is_null() ? "null" : c["command"].get<std::string>();

        if (const auto d = kSessionDivergences.find(raw); d != kSessionDivergences.end()) {
            const std::string expected_reply = d->second.cpp_reply;
            if (expected_reply == "bad_message") {
                CHECK(reply.find(R"("code":"bad_message")") != std::string::npos);
            } else {
                CHECK_EQ(reply, expected_reply);
            }
            CHECK_EQ(command, std::string(d->second.cpp_command ? d->second.cpp_command : "null"));
            continue;
        }
        if (!same_reply(reply, c["reply"])) {
            test::record_failure(__FILE__, __LINE__, "reply differs for " + raw.substr(0, 120) + "\n  node: " +
                                                         c["reply"].dump().substr(0, 200) + "\n  c++:  " + reply.substr(0, 200));
        }
        if (command != node_command) {
            test::record_failure(__FILE__, __LINE__, "command differs for " + raw.substr(0, 120) + "\n  node: " +
                                                         node_command.substr(0, 200) + "\n  c++:  " + command.substr(0, 200));
        }
        ++compared;
    }
    CHECK(compared >= 50);

    test::FakeInputSink down;
    down.up = false;
    ws::Session session(down);
    CHECK_EQ(session.on_message(golden["helperDown"]["raw"].get<std::string>()).value_or("<none>"),
             golden["helperDown"]["reply"].get<std::string>());
}

TEST_CASE(pairing_hash_token_and_lockout_match_node) {
    const json g = load("pairing.json");
    const std::string salt = g["salt"].get<std::string>();
    CHECK_EQ(auth::hash_pin(g["pin"].get<std::string>(), salt), g["pinHash"].get<std::string>());

    std::string secret;
    const std::string hex = g["secretHex"].get<std::string>();
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) secret.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    std::int64_t now = g["issuedAt"].get<std::int64_t>();
    auth::Pairing pairing(auth::StoredPin{salt, g["pinHash"].get<std::string>()}, secret, g["ttlHours"].get<std::int64_t>(),
                          [&now] { return now; });
    const auto issued = pairing.pair(g["pin"].get<std::string>());
    REQUIRE(issued.ok);
    CHECK_EQ(issued.token, g["token"].get<std::string>());  // Node-issued tokens stay valid after the switch
    for (const auto& v : g["verify"]) {
        now = v["now"].get<std::int64_t>();
        CHECK_EQ(pairing.verify_token(v["token"].get<std::string>()), v["ok"].get<bool>());
    }

    auth::Pairing lock(auth::StoredPin{salt, g["pinHash"].get<std::string>()}, secret, 24, [&now] { return now; });
    for (const auto& a : g["lockSequence"]) {
        now = a["at"].get<std::int64_t>();
        const auto r = lock.pair(a["pin"].get<std::string>());
        const json& expected = a["result"];
        CHECK_EQ(r.ok, expected["ok"].get<bool>());
        if (!r.ok) {
            CHECK_EQ(std::string(r.error == auth::PairError::locked ? "locked" : "bad_pin"), expected["error"].get<std::string>());
        }
    }
}

namespace {

struct GoldenServer {
    GoldenServer()
        : pairing(load_pairing()),
          app(sink, pairing, logger, kFixtures / "http-root" / "public"),
          server(app, logger) {}

    auth::Pairing load_pairing() {
        std::string error;
        if (!auth::store_pin(root.path(), "123456", error)) throw std::runtime_error(error);
        auto loaded = auth::load_pairing(root.path());
        return auth::Pairing(*loaded.pin, loaded.secret, 24, [] {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        });
    }

    test::TempDir root;
    test::FakeInputSink sink;
    log::NullLogger logger;
    auth::Pairing pairing;
    app::ManusApp app;
    test::ServerThread server;
};

std::string build_request(const json& c, const std::map<std::string, std::string>& etags) {
    std::string req = c["method"].get<std::string>() + " " + c["path"].get<std::string>() + " HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    if (c.contains("headers")) {
        for (const auto& [name, value] : c["headers"].items()) req += name + ": " + value.get<std::string>() + "\r\n";
    }
    if (c.contains("ifNoneMatchFrom")) {
        const auto it = etags.find(c["ifNoneMatchFrom"].get<std::string>());
        req += "If-None-Match: " + (it == etags.end() ? std::string() : it->second) + "\r\n";
    }
    std::string body;
    if (c.contains("body")) {
        body = c["body"].get<std::string>();
        req += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
    }
    return req + "Connection: close\r\n\r\n" + body;
}

json header_or_null(const test::ParsedResponse& r, const std::string& name) {
    const auto it = r.headers.find(name);
    return it == r.headers.end() ? json(nullptr) : json(it->second);
}

}  // namespace

TEST_CASE(http_responses_match_node) {
    const json g = load("http.json");
    GoldenServer s;
    std::map<std::string, std::string> etags;
    for (const auto& c : g["cases"]) {
        const std::string name = c["name"].get<std::string>();
        const auto response = test::exchange_one(s.server.port(), build_request(c, etags));
        if (!response) {
            test::record_failure(__FILE__, __LINE__, "no response for " + name);
            continue;
        }
        if (auto it = response->headers.find("etag"); it != response->headers.end()) etags[name] = it->second;
        const json& expected = c["response"];

        if (const auto d = kHttpDivergences.find(name); d != kHttpDivergences.end()) {
            CHECK_EQ(response->status, d->second.second);
            continue;
        }
        const auto fail = [&](const std::string& what, const std::string& node, const std::string& cpp) {
            test::record_failure(__FILE__, __LINE__, name + ": " + what + "\n  node: " + node + "\n  c++:  " + cpp);
        };
        if (response->status != expected["status"].get<int>()) {
            fail("status", expected["status"].dump(), std::to_string(response->status));
        }
        for (const char* h : {"content-type", "cache-control"}) {
            if (header_or_null(*response, h) != expected["headers"][h]) {
                fail(h, expected["headers"][h].dump(), header_or_null(*response, h).dump());
            }
        }
        if (header_or_null(*response, "etag").is_null() != expected["headers"]["etag"].is_null()) {
            fail("etag presence", expected["headers"]["etag"].dump(), header_or_null(*response, "etag").dump());
        }
        std::string body = response->body;
        if (name.rfind("pair_ok", 0) == 0) {
            const auto t = json::parse(body);
            const std::string token = t.value("token", "");
            const auto dot = token.find('.');
            CHECK(dot != std::string::npos && token.size() - dot - 1 == 64);
            body = R"({"token":"<token>"})";
        }
        if (body != expected["body"].get<std::string>()) fail("body", expected["body"].dump(), json(body).dump());
    }

    for (const auto& c : g["rawCases"]) {
        const std::string name = c["name"].get<std::string>();
        const std::string out = test::raw_exchange(s.server.port(), c["raw"].get<std::string>(), std::chrono::milliseconds(3000));
        const json& expected = c["response"];
        if (expected["closedWithoutResponse"].get<bool>()) {
            if (!out.empty()) test::record_failure(__FILE__, __LINE__, name + ": expected the socket to be closed without a response");
            continue;
        }
        const auto responses = test::parse_responses(out);
        if (responses.empty() || responses.front().status != expected["status"].get<int>()) {
            test::record_failure(__FILE__, __LINE__, name + ": status differs (node " + expected["status"].dump() + ")");
        }
    }
}

namespace {

using WsStream = websocket::stream<boost::asio::ip::tcp::socket>;

std::string fetch_token(unsigned short port) {
    const std::string body = R"({"pin":"123456"})";
    const auto r = test::exchange_one(port, "POST /api/pair HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: " +
                                                std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    if (!r || r->status != 200) return {};
    return json::parse(r->body)["token"].get<std::string>();
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
    return s;
}

// Order-insensitive comparison: Node acknowledged injections asynchronously, so
// a later ping's pong could overtake an earlier ack; clients match replies by seq.
std::vector<std::string> normalised(std::vector<std::string> replies) {
    for (auto& r : replies) {
        const json j = json::parse(r);
        if (j.value("code", "") == "bad_message") r = R"({"type":"error","code":"bad_message"})";
    }
    std::sort(replies.begin(), replies.end());
    return replies;
}

}  // namespace

TEST_CASE(websocket_behaviour_matches_node) {
    const json g = load("ws.json");
    GoldenServer s;
    const std::string token = fetch_token(s.server.port());
    REQUIRE(!token.empty());
    std::string tampered = token;
    tampered.back() = tampered.back() == '0' ? '1' : '0';
    std::string encoded = token;
    encoded.replace(encoded.find('.'), 1, "%2E");

    for (const auto& c : g["cases"]) {
        const std::string name = c["name"].get<std::string>();
        std::string target = c["target"].get<std::string>();
        target = replace_all(target, "<tampered>", tampered);
        target = replace_all(target, "<token-encoded>", encoded);
        target = replace_all(target, "<token>", token);
        const json& outcome = c["outcome"];

        boost::asio::io_context io;
        WsStream ws(io);
        ws.next_layer().connect({boost::asio::ip::address_v4::loopback(), s.server.port()});
        boost::system::error_code ec;
        ws.handshake("127.0.0.1", target, ec);
        CHECK_EQ(!ec, outcome["opened"].get<bool>());
        if (ec) continue;

        std::vector<std::string> sent;
        for (const auto& m : c["messages"]) {
            ws.text(true);
            ws.write(boost::asio::buffer(m.get<std::string>()), ec);
        }
        const auto& expected_replies = outcome["replies"];
        std::vector<std::string> replies;
        boost::beast::flat_buffer buffer;
        if (outcome["closeCode"].get<int>() == 4401) {
            ws.read(buffer, ec);
            CHECK(ec == websocket::error::closed);
            if (ws.reason().code != 4401) test::record_failure(__FILE__, __LINE__, name + ": expected close 4401");
            continue;
        }
        for (std::size_t i = 0; i < expected_replies.size(); ++i) {
            buffer.consume(buffer.size());
            ws.read(buffer, ec);
            if (ec) break;
            replies.push_back(boost::beast::buffers_to_string(buffer.data()));
        }
        std::vector<std::string> node;
        for (const auto& r : expected_replies) node.push_back(r.get<std::string>());
        if (normalised(replies) != normalised(node)) {
            test::record_failure(__FILE__, __LINE__, name + ": replies differ");
        }
        ws.close(websocket::close_code::normal, ec);
    }
}
