#pragma once

#include <optional>
#include <string_view>

#include "auth/pairing.hpp"
#include "http/http_reply.hpp"

namespace manus::http {

struct ApiContext {
    bool helper_up = false;              // "helper": "up" | "down" in /health (input injection available)
    auth::Pairing* pairing = nullptr;    // POST /api/pair
};

// JSON API (api.ts handleApi):
//   any method /health   -> 200 {"ok":true,"service":"manus","helper":"up"|"down"}
//   POST /api/pair       -> 200 {"token"} | 400 {"error":"bad_request"} | 401 {"error":"bad_pin"} | 429 {"error":"locked"}
// Returns nullopt for anything else, so the caller falls back to static files.
std::optional<HttpReply> handle_api(const HttpRequest& request, std::string_view pathname, ApiContext& context);

}  // namespace manus::http
