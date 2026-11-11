// libFuzzer entry: raw bytes of an HTTP/1.1 request through the same Beast parser
// settings the transport uses (header and body limits), then the request-target
// rules applied to whatever parsed.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <boost/asio/buffer.hpp>
#include <boost/beast/http.hpp>

#include "http/url.hpp"
#include "transport/limits.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    namespace bhttp = boost::beast::http;
    const manus::transport::ServerLimits limits;
    bhttp::request_parser<bhttp::string_body> parser;
    parser.header_limit(static_cast<std::uint32_t>(limits.header_limit));
    parser.body_limit(static_cast<std::uint64_t>(limits.body_limit));
    parser.eager(true);
    boost::beast::error_code ec;
    (void)parser.put(boost::asio::buffer(data, size), ec);
    if (!ec && parser.is_header_done()) {
        const auto target = parser.get().target();
        (void)manus::http::request_pathname(std::string_view(target.data(), target.size()));
    }
    return 0;
}
