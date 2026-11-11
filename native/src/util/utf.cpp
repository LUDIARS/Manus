#include "util/utf.hpp"

namespace manus::util {
namespace {

constexpr std::uint32_t kReplacement = 0xFFFD;

// Decodes one scalar value starting at i. Returns the code point and advances
// i; an invalid sequence yields U+FFFD and consumes exactly one byte.
std::uint32_t decode_one(std::string_view s, std::size_t& i, bool& valid) {
    const auto byte = [&](std::size_t at) { return static_cast<std::uint8_t>(s[at]); };
    const std::uint8_t b0 = byte(i);
    valid = true;
    if (b0 < 0x80) {
        ++i;
        return b0;
    }
    std::size_t length = 0;
    std::uint32_t cp = 0;
    std::uint32_t min = 0;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        length = 2;
        cp = b0 & 0x1Fu;
        min = 0x80;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        length = 3;
        cp = b0 & 0x0Fu;
        min = 0x800;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        length = 4;
        cp = b0 & 0x07u;
        min = 0x10000;
    } else {
        valid = false;
        ++i;
        return kReplacement;
    }
    if (s.size() - i < length) {
        valid = false;
        ++i;
        return kReplacement;
    }
    for (std::size_t k = 1; k < length; ++k) {
        const std::uint8_t b = byte(i + k);
        if ((b & 0xC0u) != 0x80u) {
            valid = false;
            ++i;
            return kReplacement;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        valid = false;
        ++i;
        return kReplacement;
    }
    i += length;
    return cp;
}

}  // namespace

std::size_t utf16_length(std::string_view utf8) {
    std::size_t units = 0;
    std::size_t i = 0;
    bool valid = true;
    while (i < utf8.size()) {
        const std::uint32_t cp = decode_one(utf8, i, valid);
        units += cp >= 0x10000 ? 2 : 1;
    }
    return units;
}

std::vector<std::uint16_t> utf8_to_utf16(std::string_view utf8) {
    std::vector<std::uint16_t> out;
    out.reserve(utf8.size());
    std::size_t i = 0;
    bool valid = true;
    while (i < utf8.size()) {
        const std::uint32_t cp = decode_one(utf8, i, valid);
        if (cp >= 0x10000) {
            const std::uint32_t v = cp - 0x10000;
            out.push_back(static_cast<std::uint16_t>(0xD800 + (v >> 10)));
            out.push_back(static_cast<std::uint16_t>(0xDC00 + (v & 0x3FF)));
        } else {
            out.push_back(static_cast<std::uint16_t>(cp));
        }
    }
    return out;
}

bool is_valid_utf8(std::string_view bytes) {
    std::size_t i = 0;
    bool valid = true;
    while (i < bytes.size()) {
        decode_one(bytes, i, valid);
        if (!valid) return false;
    }
    return true;
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace manus::util
