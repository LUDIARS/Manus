#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>

#include "http/http_reply.hpp"
#include "transport/connection_tracker.hpp"
#include "transport/server.hpp"

namespace manus::transport {

// One accepted TCP connection: reads HTTP/1.1 requests (keep-alive), answers
// them through RequestHandler, or hands the socket to WsConnection on an
// upgrade to /ws.
class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(boost::asio::ip::tcp::socket socket, std::shared_ptr<ServerContext> context);
    void start();

private:
    using Request = boost::beast::http::request<boost::beast::http::string_body>;

    void read_header();
    void on_header(boost::system::error_code ec);
    void read_body();
    void on_body(boost::system::error_code ec);
    void on_read_error(boost::system::error_code ec);
    void handle_request();
    void handle_upgrade();
    void send_reply(http::HttpReply reply, bool close);
    void send_status(unsigned status, bool close, std::string_view text = {});
    template <class Response>
    void send(std::shared_ptr<Response> response, bool close);
    void on_write(boost::system::error_code ec, bool close);
    void close_socket();
    void linger_close();
    void drain();

    std::shared_ptr<ServerContext> context_;  // declared first: destroyed last
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
    std::shared_ptr<void> pending_response_;
    bool first_request_ = true;
    bool upgraded_ = false;
    std::vector<char> drain_buffer_;
    std::size_t drained_ = 0;
    ConnectionTracker::Ticket ticket_;
};

}  // namespace manus::transport
