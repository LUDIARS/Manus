#include "auth/pairing.hpp"

#include "crypto/crypto.hpp"

namespace manus::auth {
namespace {

// The payload is always one this service signed: a plain decimal integer.
bool parse_expiry(std::string_view payload, std::int64_t& out) {
    if (payload.empty() || payload.size() > 18) return false;
    std::int64_t value = 0;
    for (const char c : payload) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

}  // namespace

bool is_valid_pin_format(std::string_view pin) {
    if (pin.size() < 4 || pin.size() > 10) return false;
    for (const char c : pin) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

std::string hash_pin(std::string_view pin, std::string_view salt) {
    std::string input;
    input.reserve(salt.size() + 1 + pin.size());
    input.append(salt);
    input.push_back(':');
    input.append(pin);
    return crypto::sha256_hex(input);
}

Pairing::Pairing(StoredPin stored, std::string secret, std::int64_t ttl_hours, EpochMsClock now)
    : stored_(std::move(stored)), secret_(std::move(secret)), ttl_ms_(ttl_hours * 3'600'000), now_(std::move(now)) {}

PairResult Pairing::pair(std::string_view pin) {
    if (now_() < locked_until_) return {false, {}, PairError::locked};
    const std::string attempt = hash_pin(pin, stored_.salt);
    if (!crypto::constant_time_equal(attempt, stored_.pin_hash)) {
        fail_count_ += 1;
        if (fail_count_ >= kMaxConsecutiveFailures) {
            locked_until_ = now_() + kLockMs;
            fail_count_ = 0;
        }
        return {false, {}, PairError::bad_pin};
    }
    fail_count_ = 0;
    return {true, issue_token(), PairError::bad_pin};
}

std::string Pairing::issue_token() const {
    const std::string payload = std::to_string(now_() + ttl_ms_);
    return payload + "." + crypto::hmac_sha256_hex(secret_, payload);
}

bool Pairing::verify_token(std::string_view token) const {
    const std::size_t dot = token.find('.');
    if (dot == std::string_view::npos || dot == 0) return false;
    const std::string_view payload = token.substr(0, dot);
    const std::string_view sig = token.substr(dot + 1);
    const std::string expected = crypto::hmac_sha256_hex(secret_, payload);
    if (!crypto::constant_time_equal(sig, expected)) return false;
    std::int64_t expiry = 0;
    if (!parse_expiry(payload, expiry)) return false;
    return now_() < expiry;
}

}  // namespace manus::auth
