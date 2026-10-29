#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "log/logger.hpp"

namespace manus::log {

struct JsonlLoggerOptions {
    std::string service = "manus";
    std::filesystem::path logs_dir;  // <logs_dir>/<service>/YYYY-MM-DD.jsonl
    int retention_days = 14;         // 0 disables the sweep
    bool console = true;             // info -> stdout, warn/error -> stderr
    bool file = true;
};

// VESTIGIUM_LOGS_DIR, or <cwd>/logs (Vestigium resolveLogsDir).
std::filesystem::path default_logs_dir();

// Console output plus a Vestigium-format JSONL file per UTC day. File errors
// are reported once on stderr and never stop the service.
class JsonlLogger final : public Logger {
public:
    explicit JsonlLogger(JsonlLoggerOptions options);
    void write(Level level, std::string_view message) override;

    // Deletes day files older than retention_days (never today's file). Runs at
    // start-up and then at most once an hour from write().
    void sweep(std::int64_t now_ms);

private:
    void ensure_open(std::int64_t now_ms);
    void report_file_error(const std::string& what);

    JsonlLoggerOptions options_;
    std::filesystem::path service_dir_;
    std::ofstream out_;
    std::int64_t current_day_ = INT64_MIN;
    std::int64_t last_sweep_ms_ = 0;
    long long pid_ = 0;
    bool error_reported_ = false;
    std::mutex mutex_;
};

}  // namespace manus::log
