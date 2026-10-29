#pragma once

#include <optional>
#include <string>

namespace manus::platform {

// Reads one environment variable. Returns nullopt when it is unset.
std::optional<std::string> get_env(const char* name);

// Process id of this process.
long long current_pid();

}  // namespace manus::platform
