#pragma once

#include <memory>
#include <string>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>

#include "transport/connection_tracker.hpp"
#include "transport/handler.hpp"
#include "transport/server.hpp"

namespace manus::transport {

// One WebSocket on /ws. After the handshake, a missing handler (unauthorised)
// closes with 4401 "unauthorized"; otherwise each message is handed to the
// handler and its reply is written before the next message is read.
class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
    WsConnection(boost::beast::tcp_stream stream, std::shared_ptr<ServerContext> context,
                 std::unique_ptr<MessageHandler> handler);
    void run(boost::beast::http::request<boost::beast::http::string_body> request);

private:
    void on_accept(boost::system::error_code ec);
    void read();
    void on_read(boost::system::error_code ec);
    void on_write(boost::system::error_code ec);
    void terminate();

    std::shared_ptr<ServerContext> context_;  // declared first: destroyed last
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer buffer_;
    std::unique_ptr<MessageHandler> handler_;
    boost::beast::http::request<boost::beast::http::string_body> request_;
    std::string reply_;
    ConnectionTracker::Ticket ticket_;
};

}  // namespace manus::transport
