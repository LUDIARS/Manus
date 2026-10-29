#include "config/config_loader.hpp"

#include "platform/environment.hpp"
#include "platform/file_read.hpp"
#include "util/bounded_json.hpp"

namespace manus::config {
namespace {
constexpr util::JsonLimits kConfigLimits{64 * 1024, 1024, 16};
}

ConfigResult load_config(const std::filesystem::path& root) {
    const auto file = root / "manus.config.json";
    nlohmann::json raw = nlohmann::json::object();
    const auto read = platform::read_file_bounded(file, kConfigLimits.max_bytes);
    switch (read.status) {
        case platform::FileReadStatus::missing:
            break;
        case platform::FileReadStatus::too_large:
            return {std::nullopt, "manus.config.json is larger than 64 KiB"};
        case platform::FileReadStatus::io_error:
            return {std::nullopt, "cannot read " + file.string()};
        case platform::FileReadStatus::ok: {
            auto parsed = util::parse_bounded_json(read.bytes, kConfigLimits);
            if (!parsed.value) return {std::nullopt, "manus.config.json: " + parsed.error};
            raw = std::move(*parsed.value);
            break;
        }
    }
    auto result = parse_config(raw);
    if (!result.config) return result;
    return apply_env_overrides(*result.config, platform::get_env("MANUS_PORT"), platform::get_env("MANUS_BIND"));
}

}  // namespace manus::config
