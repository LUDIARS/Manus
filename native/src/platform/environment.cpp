#include "platform/environment.hpp"

#include <cstdlib>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace manus::platform {

std::optional<std::string> get_env(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return std::nullopt;
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) return std::nullopt;
    return std::string(value);
#endif
}

long long current_pid() {
#ifdef _WIN32
    return static_cast<long long>(_getpid());
#else
    return static_cast<long long>(getpid());
#endif
}

}  // namespace manus::platform
