#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace manus::crypto {

// All primitives come from OpenSSL 3 (libcrypto); nothing here is hand-rolled.

// Lower-case hex SHA-256 of data.
std::string sha256_hex(std::string_view data);

// Lower-case hex HMAC-SHA256(key, data).
std::string hmac_sha256_hex(std::string_view key, std::string_view data);

// n bytes from the OpenSSL CSPRNG. Throws std::runtime_error if it fails.
std::string random_bytes(std::size_t n);

// Lower-case hex of bytes.
std::string to_hex(std::string_view bytes);

// Length comparison, then a constant-time comparison of the contents
// (timingSafeEqual after the length check in pairing.ts).
bool constant_time_equal(std::string_view a, std::string_view b);

}  // namespace manus::crypto
