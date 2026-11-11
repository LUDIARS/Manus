#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace manus::http {

// Transport-independent view of one HTTP request (the body is already bounded).
struct HttpRequest {
    std::string_view method;
    std::string_view target;
    std::string_view body;
    std::optional<std::string_view> if_none_match;
};

struct Header {
    std::string name;
    std::string value;
};

// Transport-independent response. When file is set, the transport streams that
// file (file_size bytes) as the body instead of body.
struct HttpReply {
    unsigned status = 200;
    std::vector<Header> headers;
    std::string body;
    std::optional<std::filesystem::path> file;
    std::uint64_t file_size = 0;
};

// res.writeHead(status).end(text): plain body, no content-type.
inline HttpReply text_reply(unsigned status, std::string body) {
    HttpReply reply;
    reply.status = status;
    reply.body = std::move(body);
    return reply;
}

// api.ts json(): JSON body with "application/json; charset=utf-8".
inline HttpReply json_reply(unsigned status, std::string body) {
    HttpReply reply;
    reply.status = status;
    reply.headers.push_back({"content-type", "application/json; charset=utf-8"});
    reply.body = std::move(body);
    return reply;
}

}  // namespace manus::http
