#pragma once

#include <memory>
#include <string_view>

#include "http/api.hpp"
#include "log/logger.hpp"
#include "transport/handler.hpp"

namespace manus::app {

// Routes HTTP requests and WebSocket upgrades to the business rules
// (server.ts wiring: API first, then static files; /ws needs a valid token).
class ManusApp final : public transport::RequestHandler {
public:
    ManusApp(bool helper_up, log::Logger& logger);

    http::HttpReply handle_http(const http::HttpRequest& request) override;
    std::unique_ptr<transport::MessageHandler> open_websocket(std::string_view target) override;

private:
    http::ApiContext api_;
    log::Logger& logger_;
};

}  // namespace manus::app
