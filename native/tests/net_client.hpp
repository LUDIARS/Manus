#pragma once

// Test-only network helpers: a raw HTTP exchange over TCP (bytes in, bytes out)
// and a small response parser. Everything has a deadline so a broken server
// fails the test instead of hanging it.

#include <array>
#include <cctype>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include "transport/server.hpp"

namespace manus::test {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Runs a transport::Server on its own thread, bound to 127.0.0.1:<ephemeral>.
class ServerThread {
public:
    ServerThread(transport::RequestHandler& handler, log::Logger& logger, transport::ServerLimits limits = {})
        : server_(io_, handler, logger, limits) {
        const auto ec = server_.listen(tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        if (ec) throw std::runtime_error("listen failed: " + ec.message());
        port_ = server_.local_endpoint().port();
        thread_ = std::thread([this] { io_.run(); });
    }
    ~ServerThread() {
        server_.stop();
        thread_.join();
    }
    unsigned short port() const { return port_; }
    void stop() { server_.stop(); }

private:
    asio::io_context io_;
    transport::Server server_;
    unsigned short port_ = 0;
    std::thread thread_;
};

// Connects, writes request, reads until the server closes or the deadline passes.
inline std::string raw_exchange(unsigned short port, const std::string& request,
                                std::chrono::milliseconds deadline = std::chrono::milliseconds(5000)) {
    asio::io_context io;
    tcp::socket socket(io);
    boost::system::error_code ec;
    socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port), ec);
    if (ec) return {};
    asio::write(socket, asio::buffer(request), ec);
    std::string out;
    std::array<char, 8192> buffer{};
    std::function<void()> read_more = [&] {
        socket.async_read_some(asio::buffer(buffer), [&](boost::system::error_code rec, std::size_t n) {
            out.append(buffer.data(), n);
            if (!rec) read_more();
        });
    };
    read_more();
    io.run_for(deadline);
    return out;
}

struct ParsedResponse {
    int status = 0;
    std::map<std::string, std::string> headers;  // lower-case names
    std::string body;
};

// Parses consecutive HTTP/1.1 responses (Content-Length bodies only).
inline std::vector<ParsedResponse> parse_responses(std::string data) {
    std::vector<ParsedResponse> out;
    while (!data.empty()) {
        const auto head_end = data.find("\r\n\r\n");
        if (head_end == std::string::npos) break;
        ParsedResponse r;
        const std::string head = data.substr(0, head_end);
        std::size_t line_end = head.find("\r\n");
        const std::string status_line = head.substr(0, line_end);
        if (status_line.size() >= 12) r.status = std::stoi(status_line.substr(9, 3));
        while (line_end != std::string::npos) {
            const std::size_t next = head.find("\r\n", line_end + 2);
            const std::string line = head.substr(line_end + 2, next == std::string::npos ? std::string::npos : next - line_end - 2);
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = line.substr(0, colon);
                for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                std::string value = line.substr(colon + 1);
                while (!value.empty() && value.front() == ' ') value.erase(0, 1);
                r.headers[name] = value;
            }
            line_end = next;
        }
        std::size_t length = 0;
        if (auto it = r.headers.find("content-length"); it != r.headers.end()) length = std::stoul(it->second);
        const bool interim = r.status >= 100 && r.status < 200;
        r.body = interim ? std::string() : data.substr(head_end + 4, length);
        data.erase(0, head_end + 4 + (interim ? 0 : length));
        out.push_back(std::move(r));
    }
    return out;
}

inline std::string header_of(const ParsedResponse& r, const std::string& name) {
    const auto it = r.headers.find(name);
    return it == r.headers.end() ? std::string("<none>") : it->second;
}

inline std::optional<ParsedResponse> exchange_one(unsigned short port, const std::string& request) {
    auto responses = parse_responses(raw_exchange(port, request));
    if (responses.empty()) return std::nullopt;
    return responses.back();
}

}  // namespace manus::test
