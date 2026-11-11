#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace manus::http {

// Path of a request target resolved like `new URL(target, "http://localhost").pathname`:
// query and fragment dropped, "\" treated as "/", "//host" / "scheme://host"
// authority stripped, dot segments (including %2e forms) removed, bytes >= 0x80
// percent-encoded as UTF-8 of their Latin-1 reading (what Node's parser did).
// The result stays percent-encoded.
std::string request_pathname(std::string_view target);

// First value of name in the target's query, decoded as
// application/x-www-form-urlencoded (URLSearchParams.get).
std::optional<std::string> query_param(std::string_view target, std::string_view name);

// decodeURIComponent: every %XX is decoded and the result must be valid UTF-8;
// returns nullopt where JS would throw URIError. The decoded size never
// exceeds the input size (charged against a lorica::input_budget).
std::optional<std::string> decode_uri_component(std::string_view text);

}  // namespace manus::http
