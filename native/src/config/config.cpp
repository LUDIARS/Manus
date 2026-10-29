#include "config/config.hpp"

#include <cmath>
#include <set>

namespace manus::config {
namespace {

using json = nlohmann::json;

// zod z.number().int(): a JSON number with no fractional part.
std::optional<double> integral_number(const json& v) {
    if (!v.is_number()) return std::nullopt;
    const double d = v.get<double>();
    if (!std::isfinite(d) || std::trunc(d) != d) return std::nullopt;
    return d;
}

}  // namespace

ConfigResult parse_config(const json& raw) {
    if (!raw.is_object()) return {std::nullopt, "manus.config.json: expected an object"};
    static const std::set<std::string> known = {"port", "bind", "foregroundPollMs", "tokenTtlHours", "helperDir"};
    for (const auto& item : raw.items()) {
        if (known.count(item.key()) == 0) {
            return {std::nullopt, "manus.config.json: unrecognized key '" + item.key() + "'"};
        }
    }

    ManusConfig config;
    if (raw.contains("port")) {
        const auto v = integral_number(raw["port"]);
        if (!v || *v < 1 || *v > 65535) return {std::nullopt, "manus.config.json: port must be an integer in 1..65535"};
        config.port = static_cast<std::uint16_t>(*v);
    }
    if (raw.contains("bind")) {
        if (!raw["bind"].is_string()) return {std::nullopt, "manus.config.json: bind must be a string"};
        config.bind = raw["bind"].get<std::string>();
    }
    if (raw.contains("foregroundPollMs")) {
        const auto v = integral_number(raw["foregroundPollMs"]);
        if (!v || *v < 100) return {std::nullopt, "manus.config.json: foregroundPollMs must be an integer >= 100"};
        config.foreground_poll_ms = *v;
    }
    if (raw.contains("tokenTtlHours")) {
        const auto v = integral_number(raw["tokenTtlHours"]);
        if (!v || *v < 1 || *v > static_cast<double>(kMaxTokenTtlHours)) {
            return {std::nullopt, "manus.config.json: tokenTtlHours must be an integer in 1..2000000000"};
        }
        config.token_ttl_hours = static_cast<std::int64_t>(*v);
    }
    if (raw.contains("helperDir")) {
        if (!raw["helperDir"].is_string()) return {std::nullopt, "manus.config.json: helperDir must be a string"};
        config.helper_dir = raw["helperDir"].get<std::string>();
    }
    return {config, {}};
}

ConfigResult apply_env_overrides(ManusConfig config, const std::optional<std::string>& port_env,
                                 const std::optional<std::string>& bind_env) {
    if (port_env && !port_env->empty()) {
        const std::string& s = *port_env;
        std::size_t begin = 0;
        std::size_t end = s.size();
        const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
        while (begin < end && is_space(s[begin])) ++begin;
        while (end > begin && is_space(s[end - 1])) --end;
        if (begin == end || end - begin > 5) return {std::nullopt, "MANUS_PORT must be an integer in 1..65535"};
        unsigned value = 0;
        for (std::size_t i = begin; i < end; ++i) {
            if (s[i] < '0' || s[i] > '9') return {std::nullopt, "MANUS_PORT must be an integer in 1..65535"};
            value = value * 10 + static_cast<unsigned>(s[i] - '0');
        }
        if (value < 1 || value > 65535) return {std::nullopt, "MANUS_PORT must be an integer in 1..65535"};
        config.port = static_cast<std::uint16_t>(value);
    }
    if (bind_env && !bind_env->empty()) config.bind = *bind_env;
    return {config, {}};
}

}  // namespace manus::config
