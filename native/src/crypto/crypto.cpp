#include "crypto/crypto.hpp"

#include <array>
#include <climits>
#include <stdexcept>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace manus::crypto {

std::string to_hex(std::string_view bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const char c : bytes) {
        const auto b = static_cast<unsigned char>(c);
        out += kHex[b >> 4];
        out += kHex[b & 0x0F];
    }
    return out;
}

std::string sha256_hex(std::string_view data) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("SHA-256 failed");
    }
    return to_hex(std::string_view(reinterpret_cast<const char*>(digest.data()), length));
}

std::string hmac_sha256_hex(std::string_view key, std::string_view data) {
    if (key.size() > static_cast<std::size_t>(INT_MAX)) throw std::runtime_error("HMAC key too large");
    std::array<unsigned char, EVP_MAX_MD_SIZE> mac{};
    unsigned int length = 0;
    const unsigned char* out =
        HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), reinterpret_cast<const unsigned char*>(data.data()),
             data.size(), mac.data(), &length);
    if (out == nullptr) throw std::runtime_error("HMAC-SHA256 failed");
    return to_hex(std::string_view(reinterpret_cast<const char*>(mac.data()), length));
}

std::string random_bytes(std::size_t n) {
    if (n > static_cast<std::size_t>(INT_MAX)) throw std::runtime_error("random_bytes: too many bytes");
    std::string out(n, '\0');
    if (n > 0 && RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), static_cast<int>(n)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return out;
}

bool constant_time_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace manus::crypto
