#include "http/static_server.hpp"

#include <chrono>
#include <system_error>

#include <lorica/input_budget.hpp>

#include "http/static_files.hpp"

namespace manus::http {
namespace {

constexpr const char* kCacheControl = "no-store, no-cache, must-revalidate";

std::int64_t mtime_ms(const std::filesystem::path& path, std::error_code& ec) {
    const auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    const auto sys = std::chrono::file_clock::to_sys(ftime);
    return std::chrono::duration_cast<std::chrono::milliseconds>(sys.time_since_epoch()).count();
}

}  // namespace

HttpReply serve_static(const HttpRequest& request, std::string_view pathname, const std::filesystem::path& public_root,
                       log::Logger& logger) {
    const StaticTarget target = resolve_static_path(pathname, public_root);
    switch (target.kind) {
        case StaticKind::bad_uri:
            logger.error("[http] URI malformed");
            return text_reply(500, "internal error");
        case StaticKind::forbidden:
            return text_reply(403, "forbidden");
        case StaticKind::not_found:
            return text_reply(404, "not found");
        case StaticKind::file:
            break;
    }

    std::error_code ec;
    const auto status = std::filesystem::status(target.path, ec);
    if (ec || !std::filesystem::is_regular_file(status)) return text_reply(404, "not found");
    const std::uintmax_t size = std::filesystem::file_size(target.path, ec);
    if (ec) return text_reply(404, "not found");
    // The file is streamed, but its size is still charged before it is opened.
    lorica::input_budget budget({static_cast<std::size_t>(kMaxStaticFileBytes), 1, 1, 0});
    if (size > kMaxStaticFileBytes || !budget.consume(static_cast<std::size_t>(size), 1, 1)) {
        logger.error("[http] static file larger than 64 MiB refused: " + target.path.filename().string());
        return text_reply(500, "internal error");
    }
    const std::int64_t mtime = mtime_ms(target.path, ec);
    if (ec) return text_reply(404, "not found");

    const std::string etag = make_etag(mtime, size);
    if (request.if_none_match && *request.if_none_match == etag) {
        HttpReply reply;
        reply.status = 304;
        reply.headers = {{"etag", etag}, {"cache-control", kCacheControl}};
        return reply;
    }
    HttpReply reply;
    reply.status = 200;
    reply.headers = {{"content-type", target.mime}, {"cache-control", kCacheControl}, {"etag", etag}};
    reply.file = target.path;
    reply.file_size = size;
    return reply;
}

}  // namespace manus::http
