#include "transport/http_connection.hpp"

#include <cctype>
#include <string>

#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include "http/url.hpp"
#include "transport/ws_connection.hpp"

namespace manus::transport {

namespace beast = boost::beast;
namespace bhttp = boost::beast::http;
using tcp = boost::asio::ip::tcp;

namespace {

std::string lower(beast::string_view s) {
    std::string out(s.data(), s.size());
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// llhttp's upgrade detection: a Connection header with the "upgrade" token and
// an Upgrade header (any protocol). Node routes these to the 'upgrade' event.
bool is_upgrade_request(const bhttp::request<bhttp::string_body>& req) {
    if (req.find(bhttp::field::upgrade) == req.end()) return false;
    const auto range = req.equal_range(bhttp::field::connection);
    for (auto it = range.first; it != range.second; ++it) {
        for (const auto& token : bhttp::token_list(it->value())) {
            if (beast::iequals(token, "upgrade")) return true;
        }
    }
    return false;
}

// Sec-WebSocket-Key must be base64 of 16 bytes (ws: /^[+/0-9A-Za-z]{22}==$/).
bool valid_websocket_key(beast::string_view key) {
    if (key.size() != 24 || key.substr(22) != "==") return false;
    for (std::size_t i = 0; i < 22; ++i) {
        const auto c = static_cast<unsigned char>(key[i]);
        if (!std::isalnum(c) && c != '+' && c != '/') return false;
    }
    return true;
}

bool is_benign_close(const boost::system::error_code& ec) {
    return ec == bhttp::error::end_of_stream || ec == beast::error::timeout ||
           ec == boost::asio::error::operation_aborted || ec == boost::asio::error::eof ||
           ec == boost::asio::error::connection_reset || ec == boost::asio::error::connection_aborted ||
           ec == boost::asio::error::bad_descriptor;
}

}  // namespace

HttpConnection::HttpConnection(tcp::socket socket, std::shared_ptr<ServerContext> context)
    : context_(std::move(context)),
      stream_(std::move(socket)),
      buffer_(context_->limits.header_limit + context_->limits.body_limit + 64 * 1024) {}

void HttpConnection::start() {
    std::weak_ptr<HttpConnection> weak = shared_from_this();
    ticket_ = context_->tracker.add(
        [weak] {
            if (auto self = weak.lock()) self->close_socket();
        },
        false);
    read_header();
}

void HttpConnection::read_header() {
    parser_.emplace();
    parser_->header_limit(static_cast<std::uint32_t>(context_->limits.header_limit));
    parser_->body_limit(static_cast<std::uint64_t>(context_->limits.body_limit));
    stream_.expires_after(first_request_ ? context_->limits.first_request_timeout
                                         : context_->limits.keep_alive_timeout);
    bhttp::async_read_header(stream_, buffer_, *parser_,
                             [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                                 self->on_header(ec);
                             });
}

void HttpConnection::on_header(boost::system::error_code ec) {
    if (ec) return on_read_error(ec);
    // A later request had only the keep-alive deadline for its header; once the
    // header is in, the rest of the request gets the normal request deadline.
    if (!first_request_) stream_.expires_after(context_->limits.first_request_timeout);
    const auto& req = parser_->get();
    if (is_upgrade_request(req)) return handle_upgrade();
    if (parser_->is_done()) return handle_request();

    const auto expect = req.find(bhttp::field::expect);
    if (expect != req.end() && beast::iequals(expect->value(), "100-continue")) {
        // Node answers "100 Continue" automatically before the body is sent.
        auto response = std::make_shared<bhttp::response<bhttp::empty_body>>(bhttp::status::continue_, req.version());
        pending_response_ = response;
        stream_.expires_after(context_->limits.write_timeout);
        bhttp::async_write(stream_, *response, [self = shared_from_this()](boost::system::error_code wec, std::size_t) {
            self->pending_response_.reset();
            if (wec) return self->close_socket();
            self->read_body();
        });
        return;
    }
    read_body();
}

void HttpConnection::read_body() {
    bhttp::async_read(stream_, buffer_, *parser_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
        self->on_body(ec);
    });
}

void HttpConnection::on_body(boost::system::error_code ec) {
    if (ec) return on_read_error(ec);
    handle_request();
}

void HttpConnection::on_read_error(boost::system::error_code ec) {
    if (is_benign_close(ec)) return close_socket();
    if (ec == bhttp::error::header_limit) return send_status(431, true);
    if (ec == bhttp::error::body_limit) {
        const auto& req = parser_->get();
        const std::string pathname = http::request_pathname(std::string_view(req.target().data(), req.target().size()));
        if (req.method() == bhttp::verb::post && pathname == "/api/pair") {
            // api.ts: readBody rejects past 4096 -> 400 bad_request.
            return send_reply(http::json_reply(400, R"({"error":"bad_request"})"), true);
        }
        return send_reply(http::text_reply(413, "payload too large"), true);
    }
    // Malformed request: Node's default clientError handler answers 400 and closes.
    send_status(400, true);
}

void HttpConnection::handle_request() {
    first_request_ = false;
    const auto& req = parser_->get();
    const auto method = req.method_string();
    const auto target = req.target();
    http::HttpRequest view;
    view.method = std::string_view(method.data(), method.size());
    view.target = std::string_view(target.data(), target.size());
    view.body = req.body();
    const auto inm = req.find(bhttp::field::if_none_match);
    if (inm != req.end()) view.if_none_match = std::string_view(inm->value().data(), inm->value().size());

    http::HttpReply reply;
    try {
        reply = context_->handler.handle_http(view);
    } catch (const std::exception& ex) {
        context_->logger.error(std::string("[http] ") + ex.what());
        reply = http::text_reply(500, "internal error");
    }
    send_reply(std::move(reply), !req.keep_alive());
}

void HttpConnection::handle_upgrade() {
    const auto& req = parser_->get();
    const std::string pathname = http::request_pathname(std::string_view(req.target().data(), req.target().size()));
    if (req.method() == bhttp::verb::connect || pathname != "/ws") {
        // server.ts: upgrades to anything but /ws are destroyed without a response.
        return close_socket();
    }
    if (context_->tracker.websockets() >= context_->limits.max_websockets) {
        context_->logger.warn("[ws] websocket limit reached; refusing an upgrade");
        return send_status(503, true);
    }
    // The checks ws.handleUpgrade makes before the handshake (same statuses and texts).
    if (req.method() != bhttp::verb::get) return send_status(405, true, "Invalid HTTP method");
    if (lower(req[bhttp::field::upgrade]) != "websocket") return send_status(400, true, "Invalid Upgrade header");
    if (!valid_websocket_key(req[bhttp::field::sec_websocket_key])) {
        return send_status(400, true, "Missing or invalid Sec-WebSocket-Key header");
    }
    if (req[bhttp::field::sec_websocket_version] != "13") {
        // Beast speaks version 13 only (ws also accepted the obsolete 8).
        auto response = std::make_shared<bhttp::response<bhttp::string_body>>(bhttp::status::bad_request, req.version());
        response->set(bhttp::field::sec_websocket_version, "13");
        response->set(bhttp::field::content_type, "text/html");
        response->body() = "Missing or invalid Sec-WebSocket-Version header";
        response->keep_alive(false);
        response->prepare_payload();
        return send(response, true);
    }

    std::unique_ptr<MessageHandler> handler;
    try {
        handler = context_->handler.open_websocket(std::string_view(req.target().data(), req.target().size()));
    } catch (const std::exception& ex) {
        context_->logger.error(std::string("[ws] ") + ex.what());
        return send_status(500, true);
    }
    // The socket now belongs to the WebSocket session; this object is done.
    ticket_ = ConnectionTracker::Ticket();
    auto session = std::make_shared<WsConnection>(std::move(stream_), context_, std::move(handler));
    upgraded_ = true;
    session->run(parser_->release());
}

void HttpConnection::send_status(unsigned status, bool close, std::string_view text) {
    auto response = std::make_shared<bhttp::response<bhttp::string_body>>(static_cast<bhttp::status>(status), 11);
    response->keep_alive(!close);
    if (!text.empty()) response->set(bhttp::field::content_type, "text/html");
    response->body() = std::string(text);
    response->prepare_payload();
    send(response, close);
}

void HttpConnection::send_reply(http::HttpReply reply, bool close) {
    const bool head = parser_ && parser_->get().method() == bhttp::verb::head;
    const auto status = static_cast<bhttp::status>(reply.status);
    const auto apply_headers = [&](auto& response) {
        for (const auto& h : reply.headers) response.set(h.name, h.value);
        response.keep_alive(!close);
    };

    if (reply.file) {
        bhttp::file_body::value_type body;
        beast::error_code ec;
        body.open(reinterpret_cast<const char*>(reply.file->u8string().c_str()), beast::file_mode::scan, ec);
        if (ec) return send_reply(http::text_reply(404, "not found"), close);
        if (head) {
            auto response = std::make_shared<bhttp::response<bhttp::empty_body>>(status, 11);
            apply_headers(*response);
            response->content_length(body.size());
            return send(response, close);
        }
        auto response = std::make_shared<bhttp::response<bhttp::file_body>>(status, 11);
        response->body() = std::move(body);
        apply_headers(*response);
        response->prepare_payload();
        return send(response, close);
    }
    if (reply.status == 304 || head) {
        auto response = std::make_shared<bhttp::response<bhttp::empty_body>>(status, 11);
        apply_headers(*response);
        if (reply.status != 304) response->content_length(reply.body.size());
        return send(response, close);
    }
    auto response = std::make_shared<bhttp::response<bhttp::string_body>>(status, 11);
    apply_headers(*response);
    response->body() = std::move(reply.body);
    response->prepare_payload();
    send(response, close);
}

template <class Response>
void HttpConnection::send(std::shared_ptr<Response> response, bool close) {
    pending_response_ = response;
    stream_.expires_after(context_->limits.write_timeout);
    bhttp::async_write(stream_, *response, [self = shared_from_this(), close](boost::system::error_code ec, std::size_t) {
        self->on_write(ec, close);
    });
}

void HttpConnection::on_write(boost::system::error_code ec, bool close) {
    pending_response_.reset();
    if (ec || context_->stopping) return close_socket();
    if (close) return linger_close();
    read_header();
}

void HttpConnection::linger_close() {
    // Half-close, then drain what the peer still sends (bounded in time and size)
    // so that closing with unread input does not reset the connection before the
    // client has read our response.
    boost::system::error_code ignored;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
    stream_.expires_after(std::chrono::seconds(2));
    drain_buffer_.resize(4096);
    drain();
}

void HttpConnection::drain() {
    stream_.async_read_some(boost::asio::buffer(drain_buffer_),
                            [self = shared_from_this()](boost::system::error_code ec, std::size_t n) {
                                self->drained_ += n;
                                if (ec || self->drained_ > 64 * 1024) return self->close_socket();
                                self->drain();
                            });
}

void HttpConnection::close_socket() {
    if (upgraded_) return;
    boost::system::error_code ignored;
    stream_.socket().shutdown(tcp::socket::shutdown_both, ignored);
    stream_.socket().close(ignored);
}

}  // namespace manus::transport
