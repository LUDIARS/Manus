#include "http/api.hpp"

#include <nlohmann/json.hpp>

#include "util/bounded_json.hpp"

namespace manus::http {
namespace {

// The body itself is already capped at 4096 bytes by the transport.
constexpr util::JsonLimits kPairBodyLimits{4096, 512, 32};

HttpReply error_reply(unsigned status, const char* code) {
    nlohmann::ordered_json body;
    body["error"] = code;
    return json_reply(status, body.dump());
}

HttpReply handle_pair(const HttpRequest& request, auth::Pairing& pairing) {
    const auto parsed = util::parse_bounded_json(request.body, kPairBodyLimits);
    if (!parsed.value || !parsed.value->is_object()) return error_reply(400, "bad_request");
    const auto pin = parsed.value->find("pin");
    if (pin == parsed.value->end() || !pin->is_string()) return error_reply(400, "bad_request");

    const auto result = pairing.pair(pin->get_ref<const std::string&>());
    if (result.ok) {
        nlohmann::ordered_json body;
        body["token"] = result.token;
        return json_reply(200, body.dump());
    }
    return result.error == auth::PairError::locked ? error_reply(429, "locked") : error_reply(401, "bad_pin");
}

}  // namespace

std::optional<HttpReply> handle_api(const HttpRequest& request, std::string_view pathname, ApiContext& context) {
    if (pathname == "/health") {
        nlohmann::ordered_json body;
        body["ok"] = true;
        body["service"] = "manus";
        body["helper"] = context.helper_up ? "up" : "down";
        return json_reply(200, body.dump());
    }
    if (pathname == "/api/pair" && request.method == "POST" && context.pairing != nullptr) {
        return handle_pair(request, *context.pairing);
    }
    return std::nullopt;
}

}  // namespace manus::http
