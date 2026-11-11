#include "app/manus_app.hpp"

#include "http/static_server.hpp"
#include "http/url.hpp"
#include "ws/session.hpp"

namespace manus::app {
namespace {

// Adapts the transport's per-connection handler to the session rules.
class SessionHandler final : public transport::MessageHandler {
public:
    explicit SessionHandler(inject::InputSink& sink) : session_(sink) {}
    std::optional<std::string> on_message(std::string_view raw) override { return session_.on_message(raw); }

private:
    ws::Session session_;
};

}  // namespace

ManusApp::ManusApp(inject::InputSink& sink, auth::Pairing& pairing, log::Logger& logger,
                   std::filesystem::path public_dir)
    : sink_(sink), pairing_(pairing), logger_(logger), public_dir_(std::move(public_dir)) {
    api_.pairing = &pairing_;
}

http::HttpReply ManusApp::handle_http(const http::HttpRequest& request) {
    api_.helper_up = sink_.available();
    const std::string pathname = http::request_pathname(request.target);
    if (auto reply = http::handle_api(request, pathname, api_)) return std::move(*reply);
    return http::serve_static(request, pathname, public_dir_, logger_);
}

std::unique_ptr<transport::MessageHandler> ManusApp::open_websocket(std::string_view target) {
    const auto token = http::query_param(target, "token");
    if (!token || token->empty() || !pairing_.verify_token(*token)) return nullptr;
    return std::make_unique<SessionHandler>(sink_);
}

}  // namespace manus::app
