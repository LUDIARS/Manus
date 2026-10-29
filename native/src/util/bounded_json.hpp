#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace manus::util {

// Limits for one JSON document coming from outside the process.
struct JsonLimits {
    std::size_t max_bytes;     // raw text size, checked before parsing
    std::size_t max_elements;  // values (scalars + containers + keys)
    std::size_t max_depth;     // container nesting
};

struct JsonParseResult {
    std::optional<nlohmann::json> value;
    std::string error;  // empty on success
};

// Parses text as strict JSON (RFC 8259, no comments) while charging a
// lorica::input_budget: the raw size before parsing, then every value, key
// and container as the SAX parser produces them. Parsing stops at the first
// charge that does not fit, so a hostile document never grows past the limits.
// Duplicate keys keep the last value (same as JSON.parse).
JsonParseResult parse_bounded_json(std::string_view text, const JsonLimits& limits);

}  // namespace manus::util
