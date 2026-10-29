#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "log/logger.hpp"

namespace manus::log {

// One line of the Vestigium log file format (Vestigium DESIGN.md §2.2):
// {"ts":<epoch ms>,"level":"info","service":"manus","channel":"app","msg":"...","pid":123}
struct JsonlRecord {
    std::int64_t ts = 0;
    Level level = Level::info;
    std::string_view service;
    std::string_view msg;
    std::int64_t pid = 0;
};

// Messages above this many UTF-8 bytes are cut (Vestigium MAX_MSG_BYTES).
inline constexpr std::size_t kMaxMessageBytes = 64 * 1024;

// "info" / "warn" / "error".
std::string_view level_name(Level level);

// Console hook channel mapping: warn/error -> "stderr", info -> "app".
std::string_view channel_for(Level level);

// Serialises one record as a JSON object followed by "\n". Long messages are
// truncated at a UTF-8 boundary with a "…(truncated N)" suffix.
std::string format_jsonl_record(const JsonlRecord& record);

// "YYYY-MM-DD" of the UTC day containing epoch_ms.
std::string utc_day(std::int64_t epoch_ms);

// Parses "YYYY-MM-DD.jsonl" / "YYYY-MM-DD.jsonl.gz" into days since 1970-01-01.
std::optional<std::int64_t> parse_day_file_name(std::string_view name);

// Days since 1970-01-01 of the UTC day containing epoch_ms.
std::int64_t utc_day_number(std::int64_t epoch_ms);

}  // namespace manus::log
