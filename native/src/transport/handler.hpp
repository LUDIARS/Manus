#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "http/http_reply.hpp"

namespace manus::transport {

// One authorised WebSocket connection's message handler.
class MessageHandler {
public:
    virtual ~MessageHandler() = default;
    // Returns the reply to send as one text frame, if any.
    virtual std::optional<std::string> on_message(std::string_view raw) = 0;
};

// What the transport asks of the application. Called on the I/O thread only.
class RequestHandler {
public:
    virtual ~RequestHandler() = default;
    // Any HTTP request that is not a WebSocket upgrade.
    virtual http::HttpReply handle_http(const http::HttpRequest& request) = 0;
    // Called once the WebSocket handshake on /ws has completed. nullptr means
    // "not authorised": the connection is closed with code 4401.
    virtual std::unique_ptr<MessageHandler> open_websocket(std::string_view target) = 0;
};

}  // namespace manus::transport
