#include "http/url.hpp"

#include <array>
#include <cctype>
#include <vector>

#include <lorica/input_budget.hpp>

#include "util/utf.hpp"

namespace manus::http {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

bool is_single_dot(std::string_view s) { return s == "." || iequals(s, "%2e"); }

bool is_double_dot(std::string_view s) {
    return s == ".." || iequals(s, ".%2e") || iequals(s, "%2e.") || iequals(s, "%2e%2e");
}

bool is_special_scheme(std::string_view scheme) {
    static constexpr std::array<std::string_view, 6> special = {"http", "https", "ws", "wss", "ftp", "file"};
    for (auto s : special) {
        if (iequals(scheme, s)) return true;
    }
    return false;
}

// Length of a leading "scheme:" (excluding the colon), or 0 when there is none.
std::size_t scheme_length(std::string_view s) {
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) return 0;
    for (std::size_t i = 1; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == ':') return i;
        if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') return 0;
    }
    return 0;
}

// "//authority/rest" -> "/rest" ("/" when there is no path).
std::string_view strip_authority(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && s[i] == '/') ++i;
    const std::size_t slash = s.find('/', i);
    return slash == std::string_view::npos ? std::string_view("/") : s.substr(slash);
}

std::string remove_dot_segments(std::string_view path) {
    // path starts with "/"; split the remainder on "/".
    std::vector<std::string_view> segments;
    std::size_t start = 1;
    while (true) {
        const std::size_t slash = path.find('/', start);
        segments.push_back(path.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    std::vector<std::string_view> out;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const bool last = i + 1 == segments.size();
        const auto seg = segments[i];
        if (is_double_dot(seg)) {
            if (!out.empty()) out.pop_back();
            if (last) out.emplace_back();
        } else if (is_single_dot(seg)) {
            if (last) out.emplace_back();
        } else {
            out.push_back(seg);
        }
    }
    std::string result = "/";
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i > 0) result += '/';
        result += out[i];
    }
    return result;
}

std::string percent_decode_lenient(std::string_view s, bool plus_is_space) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '+' && plus_is_space) {
            out += ' ';
        } else if (c == '%' && i + 2 < s.size() && hex_value(s[i + 1]) >= 0 && hex_value(s[i + 2]) >= 0) {
            out += static_cast<char>(hex_value(s[i + 1]) * 16 + hex_value(s[i + 2]));
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

}  // namespace

std::string request_pathname(std::string_view target) {
    target = target.substr(0, target.find('#'));
    target = target.substr(0, target.find('?'));

    std::string normalized;
    normalized.reserve(target.size());
    static constexpr char kHex[] = "0123456789ABCDEF";
    for (const char ch : target) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '\\') {
            normalized += '/';
        } else if (c >= 0x80) {
            std::string utf8;
            util::append_utf8(utf8, c);  // Node read the request line as Latin-1
            for (const char b : utf8) {
                const auto u = static_cast<unsigned char>(b);
                normalized += '%';
                normalized += kHex[u >> 4];
                normalized += kHex[u & 0x0F];
            }
        } else {
            normalized += static_cast<char>(c);
        }
    }

    std::string_view path = normalized;
    if (!path.empty() && path[0] == '/') {
        if (path.size() >= 2 && path[1] == '/') path = strip_authority(path);
        return remove_dot_segments(path);
    }
    if (const std::size_t scheme = scheme_length(path); scheme > 0) {
        const std::string_view name = path.substr(0, scheme);
        std::string_view rest = path.substr(scheme + 1);
        if (!is_special_scheme(name)) return std::string(rest);  // opaque path
        const bool has_slash = !rest.empty() && rest[0] == '/';
        if (has_slash || !iequals(name, "http")) return remove_dot_segments(strip_authority(rest));
        return remove_dot_segments("/" + std::string(rest));  // "http:foo" is relative to the base
    }
    return remove_dot_segments("/" + std::string(path));
}

std::optional<std::string> query_param(std::string_view target, std::string_view name) {
    target = target.substr(0, target.find('#'));
    const std::size_t q = target.find('?');
    if (q == std::string_view::npos) return std::nullopt;
    std::string_view query = target.substr(q + 1);
    while (!query.empty()) {
        const std::size_t amp = query.find('&');
        const std::string_view pair = query.substr(0, amp);
        query = amp == std::string_view::npos ? std::string_view() : query.substr(amp + 1);
        if (pair.empty()) continue;
        const std::size_t eq = pair.find('=');
        const std::string key = percent_decode_lenient(pair.substr(0, eq), true);
        if (key != name) continue;
        return eq == std::string_view::npos ? std::string() : percent_decode_lenient(pair.substr(eq + 1), true);
    }
    return std::nullopt;
}

std::optional<std::string> decode_uri_component(std::string_view text) {
    lorica::input_budget budget({text.size(), text.size(), 1, 0});
    if (!budget.consume(0, 0, 1)) return std::nullopt;
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    const auto read_escape = [&](std::size_t at) -> int {
        if (at + 2 >= text.size() || text[at] != '%') return -1;
        const int hi = hex_value(text[at + 1]);
        const int lo = hex_value(text[at + 2]);
        if (hi < 0 || lo < 0) return -1;
        return hi * 16 + lo;
    };
    while (i < text.size()) {
        if (text[i] != '%') {
            if (!budget.consume(1, 1, 0)) return std::nullopt;
            out += text[i++];
            continue;
        }
        const int b0 = read_escape(i);
        if (b0 < 0) return std::nullopt;
        i += 3;
        if (b0 < 0x80) {
            if (!budget.consume(1, 1, 0)) return std::nullopt;
            out += static_cast<char>(b0);
            continue;
        }
        std::size_t length = 0;
        if ((b0 & 0xE0) == 0xC0) length = 2;
        else if ((b0 & 0xF0) == 0xE0) length = 3;
        else if ((b0 & 0xF8) == 0xF0) length = 4;
        else return std::nullopt;
        std::string sequence(1, static_cast<char>(b0));
        for (std::size_t k = 1; k < length; ++k) {
            const int b = read_escape(i);
            if (b < 0 || (b & 0xC0) != 0x80) return std::nullopt;
            sequence += static_cast<char>(b);
            i += 3;
        }
        if (!util::is_valid_utf8(sequence)) return std::nullopt;  // overlong, surrogate, > U+10FFFF
        if (!budget.consume(sequence.size(), 1, 0)) return std::nullopt;
        out += sequence;
    }
    return out;
}

}  // namespace manus::http
