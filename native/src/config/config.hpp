#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace manus::config {

// manus.config.json (all keys optional). Same keys and defaults as src/config.ts.
struct ManusConfig {
    std::uint16_t port = 5390;
    std::string bind = "127.0.0.1";  // loopback by default; reached from outside via Cloudflare Tunnel
    double foreground_poll_ms = 500;  // accepted for compatibility; not used yet
    std::int64_t token_ttl_hours = 24;
    std::string helper_dir = "data/helper";  // accepted for compatibility; there is no helper process any more
};

// Largest TTL whose expiry (ms since epoch) stays a safe JS integer, so tokens
// keep the plain "<digits>.<hmac>" form the Node implementation produced.
inline constexpr std::int64_t kMaxTokenTtlHours = 2'000'000'000;

struct ConfigResult {
    std::optional<ManusConfig> config;
    std::string error;
};

// Validates the parsed file like the zod .strict() schema: unknown keys,
// wrong types and out-of-range values are errors; missing keys take defaults.
ConfigResult parse_config(const nlohmann::json& raw);

// MANUS_PORT / MANUS_BIND overrides (applied only when set and non-empty).
// The port must be a decimal integer in 1..65535 (Node passed Number(value)
// to listen() unchecked; garbage there made listen() throw at start-up).
ConfigResult apply_env_overrides(ManusConfig config, const std::optional<std::string>& port_env,
                                 const std::optional<std::string>& bind_env);

}  // namespace manus::config
