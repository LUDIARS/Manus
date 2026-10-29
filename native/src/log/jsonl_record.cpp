#include "log/jsonl_record.hpp"

#include <chrono>
#include <cstdio>

#include <nlohmann/json.hpp>

namespace manus::log {
namespace {

// Cuts at most max bytes without splitting a UTF-8 sequence.
std::size_t utf8_cut(std::string_view s, std::size_t max) {
    if (s.size() <= max) return s.size();
    std::size_t cut = max;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0u) == 0x80u) --cut;
    return cut;
}

}  // namespace

std::string_view level_name(Level level) {
    switch (level) {
        case Level::info: return "info";
        case Level::warn: return "warn";
        case Level::error: return "error";
    }
    return "info";
}

std::string_view channel_for(Level level) {
    return level == Level::info ? "app" : "stderr";
}

std::string format_jsonl_record(const JsonlRecord& record) {
    std::string msg(record.msg);
    if (msg.size() > kMaxMessageBytes) {
        const std::size_t keep = utf8_cut(msg, kMaxMessageBytes - 32);
        const std::size_t dropped = msg.size() - keep;
        msg.resize(keep);
        msg += "\xE2\x80\xA6(truncated " + std::to_string(dropped) + ")";
    }
    nlohmann::ordered_json line;
    line["ts"] = record.ts;
    line["level"] = level_name(record.level);
    line["service"] = record.service;
    line["channel"] = channel_for(record.level);
    line["msg"] = std::move(msg);
    line["pid"] = record.pid;
    return line.dump(-1, ' ', false, nlohmann::ordered_json::error_handler_t::replace) + "\n";
}

std::int64_t utc_day_number(std::int64_t epoch_ms) {
    using namespace std::chrono;
    const auto tp = sys_time<milliseconds>(milliseconds(epoch_ms));
    return floor<days>(tp).time_since_epoch().count();
}

std::string utc_day(std::int64_t epoch_ms) {
    using namespace std::chrono;
    const year_month_day ymd{sys_days(days(utc_day_number(epoch_ms)))};
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02u", static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
    return buffer;
}

std::optional<std::int64_t> parse_day_file_name(std::string_view name) {
    std::string_view rest;
    if (name.size() == 16 && name.substr(10) == ".jsonl") {
        rest = name.substr(0, 10);
    } else if (name.size() == 19 && name.substr(10) == ".jsonl.gz") {
        rest = name.substr(0, 10);
    } else {
        return std::nullopt;
    }
    const auto digits = [&](std::size_t from, std::size_t count, int& out) {
        out = 0;
        for (std::size_t i = from; i < from + count; ++i) {
            if (rest[i] < '0' || rest[i] > '9') return false;
            out = out * 10 + (rest[i] - '0');
        }
        return true;
    };
    int y = 0;
    int m = 0;
    int d = 0;
    if (rest[4] != '-' || rest[7] != '-' || !digits(0, 4, y) || !digits(5, 2, m) || !digits(8, 2, d)) {
        return std::nullopt;
    }
    using namespace std::chrono;
    const year_month_day ymd{year(y), month(static_cast<unsigned>(m)), day(static_cast<unsigned>(d))};
    if (!ymd.ok()) return std::nullopt;
    return sys_days(ymd).time_since_epoch().count();
}

}  // namespace manus::log
