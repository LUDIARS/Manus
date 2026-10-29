#pragma once

#include <memory>
#include <string_view>

#include "auth/pairing.hpp"
#include "http/api.hpp"
#include "inject/input_sink.hpp"
#include "log/logger.hpp"
#include "transport/handler.hpp"

namespace manus::app {

// Routes HTTP requests and WebSocket upgrades to the business rules
// (server.ts wiring: API first, then static files; /ws needs a valid token).
class ManusApp final : public transport::RequestHandler {
public:
    ManusApp(inject::InputSink& sink, auth::Pairing& pairing, log::Logger& logger);

    http::HttpReply handle_http(const http::HttpRequest& request) override;
    // Session.authorize: a missing, empty or invalid ?token= closes with 4401.
    std::unique_ptr<transport::MessageHandler> open_websocket(std::string_view target) override;

private:
    inject::InputSink& sink_;
    auth::Pairing& pairing_;
    log::Logger& logger_;
    http::ApiContext api_;
};

}  // namespace manus::app
