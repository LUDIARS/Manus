#pragma once

#include <string_view>

namespace manus::log {

enum class Level { info, warn, error };

// Log sink used by every layer. Messages must never contain PINs, tokens or
// the session secret.
class Logger {
public:
    virtual ~Logger() = default;
    virtual void write(Level level, std::string_view message) = 0;

    void info(std::string_view message) { write(Level::info, message); }
    void warn(std::string_view message) { write(Level::warn, message); }
    void error(std::string_view message) { write(Level::error, message); }
};

// Discards everything (tests).
class NullLogger final : public Logger {
public:
    void write(Level, std::string_view) override {}
};

}  // namespace manus::log
