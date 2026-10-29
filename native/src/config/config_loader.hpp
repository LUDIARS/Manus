#pragma once

#include <filesystem>

#include "config/config.hpp"

namespace manus::config {

// Reads <root>/manus.config.json (optional) and applies MANUS_PORT / MANUS_BIND.
ConfigResult load_config(const std::filesystem::path& root);

}  // namespace manus::config
