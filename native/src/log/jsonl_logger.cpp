#include "log/jsonl_logger.hpp"

#include <chrono>
#include <climits>
#include <iostream>
#include <system_error>

#include "log/jsonl_record.hpp"
#include "platform/environment.hpp"

namespace manus::log {
namespace {

std::int64_t now_epoch_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

constexpr std::int64_t kSweepIntervalMs = 60LL * 60 * 1000;

}  // namespace

std::filesystem::path default_logs_dir() {
    const auto env = platform::get_env("VESTIGIUM_LOGS_DIR");
    if (env && env->find_first_not_of(" \t\r\n") != std::string::npos) return std::filesystem::absolute(*env);
    std::error_code ec;
    return std::filesystem::current_path(ec) / "logs";
}

JsonlLogger::JsonlLogger(JsonlLoggerOptions options)
    : options_(std::move(options)),
      service_dir_(options_.logs_dir / options_.service),
      pid_(platform::current_pid()) {
    if (options_.file) {
        std::lock_guard<std::mutex> lock(mutex_);
        sweep(now_epoch_ms());
    }
}

void JsonlLogger::report_file_error(const std::string& what) {
    if (error_reported_) return;
    error_reported_ = true;
    std::cerr << "[vestigium] log file write failed: " << what << std::endl;
}

void JsonlLogger::ensure_open(std::int64_t now_ms) {
    const std::int64_t day = utc_day_number(now_ms);
    if (day == current_day_ && out_.is_open() && out_.good()) return;
    if (out_.is_open()) out_.close();
    std::error_code ec;
    std::filesystem::create_directories(service_dir_, ec);
    const auto file = service_dir_ / (utc_day(now_ms) + ".jsonl");
    out_.open(file, std::ios::binary | std::ios::app);
    if (!out_) {
        report_file_error("cannot open " + file.string());
        return;
    }
    current_day_ = day;
}

void JsonlLogger::write(Level level, std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (options_.console) {
        auto& stream = level == Level::info ? std::cout : std::cerr;
        stream << message << std::endl;
    }
    if (!options_.file) return;
    const std::int64_t now = now_epoch_ms();
    if (now - last_sweep_ms_ >= kSweepIntervalMs) sweep(now);
    ensure_open(now);
    if (!out_.is_open()) return;
    JsonlRecord record;
    record.ts = now;
    record.level = level;
    record.service = options_.service;
    record.msg = message;
    record.pid = pid_;
    out_ << format_jsonl_record(record);
    out_.flush();
    if (!out_) {
        report_file_error("write error");
        out_.close();
    } else {
        error_reported_ = false;
    }
}

void JsonlLogger::sweep(std::int64_t now_ms) {
    last_sweep_ms_ = now_ms;
    if (options_.retention_days <= 0) return;
    std::error_code ec;
    if (!std::filesystem::is_directory(service_dir_, ec)) return;
    const std::string today = utc_day(now_ms);
    const std::int64_t cutoff = utc_day_number(now_ms) - options_.retention_days;
    for (std::filesystem::directory_iterator it(service_dir_, ec), end; !ec && it != end; it.increment(ec)) {
        // Only plain files (a symlink is never followed or removed).
        const auto status = it->symlink_status(ec);
        if (ec || !std::filesystem::is_regular_file(status)) continue;
        const std::string name = it->path().filename().string();
        const auto day = parse_day_file_name(name);
        if (!day || name.rfind(today, 0) == 0) continue;
        if (*day < cutoff) {
            std::error_code remove_ec;
            std::filesystem::remove(it->path(), remove_ec);
        }
    }
}

}  // namespace manus::log
