#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "http/http_reply.hpp"
#include "log/logger.hpp"

namespace manus::http {

// Files above this size are not served (500).
inline constexpr std::uint64_t kMaxStaticFileBytes = 64ULL * 1024 * 1024;

// serveStatic of static-server.ts: resolves the path (static_files), stats the
// file, and answers 200 (streamed file) / 304 (If-None-Match equals the ETag) /
// 403 / 404 / 500. Every response carries "cache-control: no-store, no-cache,
// must-revalidate" when a file is involved; error bodies have no content-type.
HttpReply serve_static(const HttpRequest& request, std::string_view pathname, const std::filesystem::path& public_root,
                       log::Logger& logger);

}  // namespace manus::http
