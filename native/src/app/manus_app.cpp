#include "app/manus_app.hpp"

#include "http/url.hpp"

namespace manus::app {

ManusApp::ManusApp(bool helper_up, auth::Pairing& pairing, log::Logger& logger) : pairing_(pairing), logger_(logger) {
    api_.helper_up = helper_up;
    api_.pairing = &pairing_;
}

http::HttpReply ManusApp::handle_http(const http::HttpRequest& request) {
    const std::string pathname = http::request_pathname(request.target);
    if (auto reply = http::handle_api(request, pathname, api_)) return std::move(*reply);
    return http::text_reply(404, "not found");
}

std::unique_ptr<transport::MessageHandler> ManusApp::open_websocket(std::string_view) {
    return nullptr;  // no session handling yet: every connection is closed with 4401
}

}  // namespace manus::app
