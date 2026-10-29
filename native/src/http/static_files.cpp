#include "http/static_files.hpp"

#include <array>
#include <cctype>
#include <vector>

#include "http/url.hpp"

namespace manus::http {
namespace {

bool is_separator(char c) { return c == '/' || c == '\\'; }

std::string to_base36(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (value == 0) return "0";
    std::string out;
    while (value > 0) {
        out.insert(out.begin(), kDigits[value % 36]);
        value /= 36;
    }
    return out;
}

// CON, PRN, AUX, NUL, COM1-9, LPT1-9 (also with an extension, any case).
bool is_windows_device_name(std::string_view segment) {
    std::string base(segment.substr(0, segment.find('.')));
    while (!base.empty() && (base.back() == ' ' || base.back() == '.')) base.pop_back();
    for (auto& c : base) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") return true;
    if (base.size() == 4 && (base.rfind("COM", 0) == 0 || base.rfind("LPT", 0) == 0) && base[3] >= '1' && base[3] <= '9') {
        return true;
    }
    return false;
}

}  // namespace

std::string_view node_extname(std::string_view path) {
    // Port of Node's path.extname (win32 separators, no drive prefix in URL paths).
    std::size_t start_dot = std::string_view::npos;
    std::size_t start_part = 0;
    std::size_t end = std::string_view::npos;
    bool matched_slash = true;
    int pre_dot_state = 0;
    for (std::size_t k = path.size(); k-- > 0;) {
        const char c = path[k];
        if (is_separator(c)) {
            if (!matched_slash) {
                start_part = k + 1;
                break;
            }
            continue;
        }
        if (end == std::string_view::npos) {
            matched_slash = false;
            end = k + 1;
        }
        if (c == '.') {
            if (start_dot == std::string_view::npos) start_dot = k;
            else if (pre_dot_state != 1) pre_dot_state = 1;
        } else if (start_dot != std::string_view::npos) {
            pre_dot_state = -1;
        }
    }
    if (start_dot == std::string_view::npos || end == std::string_view::npos || pre_dot_state == 0 ||
        (pre_dot_state == 1 && start_dot == end - 1 && start_dot == start_part + 1)) {
        return {};
    }
    return path.substr(start_dot, end - start_dot);
}

std::string mime_type_for(std::string_view ext) {
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".webmanifest") return "application/manifest+json";
    return "application/octet-stream";
}

std::string make_etag(std::int64_t mtime_ms, std::uint64_t size) {
    const std::string mtime = mtime_ms < 0 ? "-" + to_base36(static_cast<std::uint64_t>(-(mtime_ms + 1)) + 1)
                                           : to_base36(static_cast<std::uint64_t>(mtime_ms));
    return "\"" + mtime + "-" + to_base36(size) + "\"";
}

StaticTarget resolve_static_path(std::string_view pathname, const std::filesystem::path& public_root) {
    const auto decoded_opt = decode_uri_component(pathname);
    if (!decoded_opt) return {StaticKind::bad_uri, {}, {}};
    std::string decoded = *decoded_opt;
    if (decoded == "/" || node_extname(decoded).empty()) decoded = "/index.html";

    std::vector<std::string_view> segments;
    bool bad_name = false;
    std::size_t start = 0;
    const std::string_view view = decoded;
    while (start <= view.size()) {
        std::size_t stop = start;
        while (stop < view.size() && !is_separator(view[stop])) ++stop;
        const std::string_view segment = view.substr(start, stop - start);
        if (segment.empty() || segment == ".") {
            // nothing
        } else if (segment == "..") {
            if (segments.empty()) return {StaticKind::forbidden, {}, {}};
            segments.pop_back();
        } else {
            if (segment.find('\0') != std::string_view::npos || segment.find(':') != std::string_view::npos ||
                is_windows_device_name(segment)) {
                bad_name = true;
            }
            segments.push_back(segment);
        }
        start = stop + 1;
    }
    if (bad_name || segments.empty() || is_separator(decoded.back())) return {StaticKind::not_found, {}, {}};

    std::filesystem::path path = public_root;
    for (const auto segment : segments) path /= std::filesystem::path(std::u8string(segment.begin(), segment.end()));
    return {StaticKind::file, path, mime_type_for(node_extname(segments.back()))};
}

}  // namespace manus::http
