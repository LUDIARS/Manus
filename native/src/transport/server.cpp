#include "transport/server.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/socket_base.hpp>

#include "transport/http_connection.hpp"

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace manus::transport {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

Server::Server(asio::io_context& io, RequestHandler& handler, log::Logger& logger, ServerLimits limits)
    : io_(io), acceptor_(io), context_(std::make_shared<ServerContext>(handler, logger, limits)) {}

boost::system::error_code Server::listen(const tcp::endpoint& endpoint) {
    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) return ec;
#ifdef _WIN32
    // SO_REUSEADDR on Windows would let another process take over the port.
    BOOL exclusive = TRUE;
    if (::setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&exclusive), static_cast<int>(sizeof exclusive)) != 0) {
        return boost::system::error_code(::WSAGetLastError(), boost::system::system_category());
    }
#else
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) return ec;
#endif
    if (endpoint.address().is_v6()) {
        // Node/libuv listen on "::" as dual-stack.
        acceptor_.set_option(asio::ip::v6_only(false), ec);
        ec.clear();
    }
    acceptor_.bind(endpoint, ec);
    if (ec) return ec;
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) return ec;
    accept();
    return {};
}

tcp::endpoint Server::local_endpoint() const {
    boost::system::error_code ec;
    return acceptor_.local_endpoint(ec);
}

void Server::accept() {
    acceptor_.async_accept([this, context = context_](boost::system::error_code ec, tcp::socket socket) {
        if (ec) {
            if (ec == asio::error::operation_aborted || context->stopping) return;
            context->logger.warn(std::string("[http] accept failed: ") + ec.message());
            accept();
            return;
        }
        if (context->stopping) return;
        if (context->tracker.total() >= context->limits.max_connections) {
            // Refuse: the connection cap is reached.
            context->logger.warn("[http] connection limit reached; refusing a connection");
            boost::system::error_code ignored;
            socket.close(ignored);
        } else {
            std::make_shared<HttpConnection>(std::move(socket), context)->start();
        }
        accept();
    });
}

void Server::stop() {
    asio::post(io_, [this, context = context_] {
        context->stopping = true;
        boost::system::error_code ignored;
        acceptor_.close(ignored);
        context->tracker.close_all();
    });
}

}  // namespace manus::transport
