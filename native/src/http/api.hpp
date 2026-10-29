#pragma once

#include <optional>

#include "http/http_reply.hpp"

namespace manus::http {

struct ApiContext {
    bool helper_up = false;  // "helper": "up" | "down" in /health (input injection available)
};

// JSON API (api.ts handleApi). Returns nullopt when the request is not an API
// route, so the caller falls back to static files.
std::optional<HttpReply> handle_api(const HttpRequest& request, std::string_view pathname, ApiContext& context);

}  // namespace manus::http
