#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include "log/logger.hpp"
#include "transport/connection_tracker.hpp"
#include "transport/handler.hpp"
#include "transport/limits.hpp"

namespace manus::transport {

// State shared by the listener and its connections. Connections keep it alive
// through a shared_ptr, so it outlives every pending handler.
struct ServerContext {
    ServerContext(RequestHandler& h, log::Logger& l, ServerLimits lim) : handler(h), logger(l), limits(lim) {}
    RequestHandler& handler;
    log::Logger& logger;
    ServerLimits limits;
    ConnectionTracker tracker;
    bool stopping = false;
};

// HTTP/1.1 + WebSocket listener. Runs on one io_context thread.
class Server {
public:
    Server(boost::asio::io_context& io, RequestHandler& handler, log::Logger& logger, ServerLimits limits = {});

    // Binds and starts accepting. On Windows the socket is exclusive
    // (SO_EXCLUSIVEADDRUSE) so no other process can bind the same port.
    boost::system::error_code listen(const boost::asio::ip::tcp::endpoint& endpoint);
    boost::asio::ip::tcp::endpoint local_endpoint() const;

    // Stops accepting and terminates every connection. Safe to call from any
    // thread (it is posted to the I/O thread).
    void stop();

private:
    void accept();

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<ServerContext> context_;
};

}  // namespace manus::transport
