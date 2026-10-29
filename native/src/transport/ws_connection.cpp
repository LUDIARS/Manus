#include "transport/ws_connection.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/websocket.hpp>

namespace manus::transport {

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

WsConnection::WsConnection(beast::tcp_stream stream, std::shared_ptr<ServerContext> context,
                           std::unique_ptr<MessageHandler> handler)
    : context_(std::move(context)), ws_(std::move(stream)), handler_(std::move(handler)) {}

void WsConnection::run(beast::http::request<beast::http::string_body> request) {
    std::weak_ptr<WsConnection> weak = shared_from_this();
    ticket_ = context_->tracker.add(
        [weak] {
            if (auto self = weak.lock()) self->terminate();
        },
        true);
    request_ = std::move(request);

    // The websocket stream has its own deadlines from here on.
    beast::get_lowest_layer(ws_).expires_never();
    websocket::stream_base::timeout timeouts{};
    timeouts.handshake_timeout = context_->limits.ws_handshake_timeout;
    timeouts.idle_timeout = context_->limits.ws_idle_timeout;
    timeouts.keep_alive_pings = true;
    ws_.set_option(timeouts);
    ws_.read_message_max(context_->limits.ws_message_limit);
    // No permessage-deflate (ws' server default is off as well): nothing is ever inflated.
    websocket::permessage_deflate deflate;
    deflate.server_enable = false;
    deflate.client_enable = false;
    ws_.set_option(deflate);
    // Do not advertise the library and its version in the 101 response (Beast
    // fills in its own Server value when the decorator leaves the field unset).
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) { res.set(beast::http::field::server, "manus"); }));

    ws_.async_accept(request_, [self = shared_from_this()](boost::system::error_code ec) { self->on_accept(ec); });
}

void WsConnection::on_accept(boost::system::error_code ec) {
    if (ec) return terminate();
    if (!handler_) {
        // Same as Session.authorize: close(4401, "unauthorized") after the handshake.
        websocket::close_reason reason(static_cast<std::uint16_t>(4401));
        reason.reason = "unauthorized";
        ws_.async_close(reason, [self = shared_from_this()](boost::system::error_code) {});
        return;
    }
    read();
}

void WsConnection::read() {
    ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) { self->on_read(ec); });
}

void WsConnection::on_read(boost::system::error_code ec) {
    if (ec) return;  // closed by the peer, idle timeout, too big (1009), or terminated
    const std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    std::optional<std::string> reply;
    try {
        reply = handler_->on_message(message);
    } catch (const std::exception& ex) {
        context_->logger.error(std::string("[ws] ") + ex.what());
        return terminate();
    }
    if (!reply) return read();
    reply_ = std::move(*reply);
    ws_.text(true);
    ws_.async_write(boost::asio::buffer(reply_),
                    [self = shared_from_this()](boost::system::error_code wec, std::size_t) { self->on_write(wec); });
}

void WsConnection::on_write(boost::system::error_code ec) {
    if (ec) return;
    read();
}

void WsConnection::terminate() {
    boost::system::error_code ignored;
    auto& socket = beast::get_lowest_layer(ws_).socket();
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket.close(ignored);
}

}  // namespace manus::transport
