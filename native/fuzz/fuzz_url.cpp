// libFuzzer entry: a request-target (pathname resolution, query decoding) and
// the session token taken from it.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "auth/pairing.hpp"
#include "http/url.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view target(reinterpret_cast<const char*>(data), size);
    (void)manus::http::request_pathname(target);
    (void)manus::http::decode_uri_component(target);
    const auto token = manus::http::query_param(target, "token");
    if (token) {
        static const manus::auth::Pairing pairing({"salt", "hash"}, std::string(32, 's'), 24,
                                                  [] { return std::int64_t{1'700'000'000'000}; });
        (void)pairing.verify_token(*token);
    }
    return 0;
}
