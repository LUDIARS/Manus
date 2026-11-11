#include "transport/ws_connection.hpp"

#include <cstddef>
#include <string_view>

#include <boost/asio/buffer.hpp>
#include <boost/beast/websocket.hpp>
#include <lorica/byte_view.hpp>

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
    // read_message_max bounds the receive buffer to one message. flat_buffer's own
    // max_size is not set: Beast would then fail with buffer_overflow instead of
    // closing with 1009 (manus.server websocket_message_limit_exact_and_plus_one).
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
    // flat_buffer is contiguous: the handler reads the message in place (no copy).
    const auto data = buffer_.data();
    const auto message = lorica::byte_view::make(static_cast<const std::byte*>(data.data()), data.size(),
                                                 context_->limits.ws_message_limit);
    if (!message) return close_too_big();

    std::optional<std::string> reply;
    try {
        reply = handler_->on_message(std::string_view(static_cast<const char*>(data.data()), data.size()));
    } catch (const std::exception& ex) {
        buffer_.consume(buffer_.size());
        context_->logger.error(std::string("[ws] ") + ex.what());
        return terminate();
    }
    buffer_.consume(buffer_.size());
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

void WsConnection::close_too_big() {
    buffer_.consume(buffer_.size());
    websocket::close_reason reason(websocket::close_code::too_big);
    ws_.async_close(reason, [self = shared_from_this()](boost::system::error_code) {});
}

void WsConnection::terminate() {
    boost::system::error_code ignored;
    auto& socket = beast::get_lowest_layer(ws_).socket();
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket.close(ignored);
}

}  // namespace manus::transport
