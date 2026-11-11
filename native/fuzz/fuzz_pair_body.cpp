// libFuzzer entry: the body of POST /api/pair (JSON, PIN check, lock-out).
// The stored PIN, secret and clock are fixed test values.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "auth/pairing.hpp"
#include "http/api.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const std::string kSalt = "00000000000000000000000000000000";
    manus::auth::Pairing pairing({kSalt, manus::auth::hash_pin("123456", kSalt)}, std::string(32, 's'), 24,
                                 [] { return std::int64_t{1'700'000'000'000}; });
    manus::http::ApiContext context;
    context.pairing = &pairing;
    manus::http::HttpRequest request;
    request.method = "POST";
    request.target = "/api/pair";
    request.body = std::string_view(reinterpret_cast<const char*>(data), size);
    (void)manus::http::handle_api(request, "/api/pair", context);
    return 0;
}
