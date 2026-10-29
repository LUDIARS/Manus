#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace manus::auth {

// The pairing rules of src/auth/pairing.ts, without any file or clock access.

// ^\d{4,10}$ with ASCII digits only (JS \d without the u flag).
bool is_valid_pin_format(std::string_view pin);

// sha256("<salt>:<pin>") as lower-case hex (pairing.ts hashPin).
std::string hash_pin(std::string_view pin, std::string_view salt);

// Contents of data/pairing.json.
struct StoredPin {
    std::string salt;
    std::string pin_hash;
};

enum class PairError { locked, bad_pin };

struct PairResult {
    bool ok = false;
    std::string token;                  // when ok
    PairError error = PairError::bad_pin;  // when !ok
};

inline constexpr int kMaxConsecutiveFailures = 5;
inline constexpr std::int64_t kLockMs = 60'000;

using EpochMsClock = std::function<std::int64_t()>;

// PIN check with lock-out, and HMAC-SHA256 session tokens "<expiry ms>.<hex mac>".
// State (failure count, lock) is process-wide, as in Node.
class Pairing {
public:
    Pairing(StoredPin stored, std::string secret, std::int64_t ttl_hours, EpochMsClock now);

    // 5 consecutive failures lock pairing for 60 s; while locked every attempt is
    // "locked" and is not counted. Success resets the failure count.
    PairResult pair(std::string_view pin);

    // Valid when the MAC matches (constant-time) and the expiry is in the future.
    bool verify_token(std::string_view token) const;

private:
    std::string issue_token() const;

    StoredPin stored_;
    std::string secret_;
    std::int64_t ttl_ms_;
    EpochMsClock now_;
    int fail_count_ = 0;
    std::int64_t locked_until_ = 0;
};

}  // namespace manus::auth
