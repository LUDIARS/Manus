#include "http/api.hpp"

#include <nlohmann/json.hpp>

namespace manus::http {

std::optional<HttpReply> handle_api(const HttpRequest&, std::string_view pathname, ApiContext& context) {
    if (pathname == "/health") {
        nlohmann::ordered_json body;
        body["ok"] = true;
        body["service"] = "manus";
        body["helper"] = context.helper_up ? "up" : "down";
        return json_reply(200, body.dump());
    }
    return std::nullopt;
}

}  // namespace manus::http
