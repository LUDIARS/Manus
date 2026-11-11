#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace manus::http {

// Path rules of src/http/static-server.ts, without touching the filesystem.

// path.win32.extname: extension of the last path component ("/" and "\" are
// separators, trailing separators ignored, leading-dot names have none).
std::string_view node_extname(std::string_view path);

// The MIME table of static-server.ts (case-sensitive); everything else is
// application/octet-stream.
std::string mime_type_for(std::string_view extension);

// "\"<mtime ms base36>-<size base36>\"" (the shape static-server.ts used).
std::string make_etag(std::int64_t mtime_ms, std::uint64_t size);

enum class StaticKind {
    file,       // serve path (if it is a regular file)
    forbidden,  // 403: resolves outside public/
    not_found,  // 404
    bad_uri,    // 500: decodeURIComponent would throw
};

struct StaticTarget {
    StaticKind kind = StaticKind::not_found;
    std::filesystem::path path;
    std::string mime;
};

// pathname is the (still percent-encoded) URL pathname. It is decoded, "/" and
// extension-less paths become "/index.html", and the segments are resolved
// lexically under public_root. Leaving public_root is forbidden (checked per
// segment, so a sibling such as "public-old/" is never reachable); NUL, ":"
// (drive letters, NTFS streams), Windows device names and a trailing separator
// never name a servable file.
StaticTarget resolve_static_path(std::string_view pathname, const std::filesystem::path& public_root);

}  // namespace manus::http
