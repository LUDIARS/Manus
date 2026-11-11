#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace manus::util {

// Number of UTF-16 code units the UTF-8 text occupies (what a JS string's
// .length reports). Invalid sequences count as one unit per replaced byte,
// like U+FFFD replacement would.
std::size_t utf16_length(std::string_view utf8);

// UTF-8 -> UTF-16 code units. Invalid bytes become U+FFFD.
std::vector<std::uint16_t> utf8_to_utf16(std::string_view utf8);

// True when the bytes are well-formed UTF-8 (no overlongs, no surrogates,
// nothing above U+10FFFF).
bool is_valid_utf8(std::string_view bytes);

// Appends the UTF-8 encoding of a code point (must be a valid scalar value).
void append_utf8(std::string& out, std::uint32_t code_point);

}  // namespace manus::util
