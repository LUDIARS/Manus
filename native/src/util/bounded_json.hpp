#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <lorica/input_budget.hpp>
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

// The budget one JSON document needs: its raw text (at most max_bytes) plus the
// decoded strings and keys (never more than the raw text), max_elements values,
// one allocation for the text plus one per value, and max_depth nesting.
// Sizes are computed with lorica::checked_*; an overflow yields an empty budget.
lorica::budget_limits json_budget_limits(const JsonLimits& limits);

// Same as above, charging the caller's budget for the request or message the
// document belongs to (one budget from the start to the end of that request).
// Text longer than limits.max_bytes is refused before anything is charged.
JsonParseResult parse_bounded_json(std::string_view text, const JsonLimits& limits, lorica::input_budget& budget);

}  // namespace manus::util
