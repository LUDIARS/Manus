// Ports src/auth/pairing.test.ts (same cases, same names in the comments) and adds
// boundary cases for the lock-out, the PIN format and the stored files.
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "auth/pairing.hpp"
#include "auth/pairing_store.hpp"
#include "crypto/crypto.hpp"
#include "temp_dir.hpp"
#include "test_harness.hpp"

using namespace manus::auth;
using manus::test::TempDir;

namespace {

Pairing open_pairing(const std::filesystem::path& root, std::int64_t ttl_hours, EpochMsClock clock) {
    auto loaded = load_pairing(root);
    if (!loaded.pin) throw std::runtime_error(loaded.error);
    return Pairing(*loaded.pin, loaded.secret, ttl_hours, std::move(clock));
}

std::int64_t fixed_now = 1'000'000;
EpochMsClock fixed_clock() {
    return [] { return fixed_now; };
}

std::string set_pin(const TempDir& dir, const std::string& pin) {
    std::string error;
    if (!store_pin(dir.path(), pin, error)) throw std::runtime_error(error);
    return error;
}

}  // namespace

// "PIN 未設定なら fail-fast する"
TEST_CASE(fails_fast_without_pin) {
    TempDir dir;
    const auto loaded = load_pairing(dir.path());
    CHECK(!loaded.pin.has_value());
    CHECK(loaded.error.find("未設定") != std::string::npos);
}

// "正しい PIN でトークンを発行し検証が通る"
TEST_CASE(correct_pin_issues_verifiable_token) {
    TempDir dir;
    set_pin(dir, "123456");
    Pairing p = open_pairing(dir.path(), 24, fixed_clock());
    const auto result = p.pair("123456");
    REQUIRE(result.ok);
    CHECK(p.verify_token(result.token));
    // "<expiry ms>.<64 hex>"
    CHECK_EQ(result.token.substr(0, result.token.find('.')), std::to_string(fixed_now + 24 * 3'600'000LL));
    CHECK_EQ(result.token.size() - result.token.find('.') - 1, std::size_t{64});
}

// "誤った PIN は拒否し、5 回でロックする"
TEST_CASE(wrong_pin_rejected_and_locks_after_five) {
    TempDir dir;
    set_pin(dir, "123456");
    Pairing p = open_pairing(dir.path(), 24, fixed_clock());
    for (int i = 0; i < 5; ++i) {
        const auto r = p.pair("000000");
        CHECK(!r.ok);
        CHECK(r.error == PairError::bad_pin);
    }
    const auto locked = p.pair("123456");
    CHECK(!locked.ok);
    CHECK(locked.error == PairError::locked);
}

// "改竄トークンを拒否する"
TEST_CASE(tampered_token_rejected) {
    TempDir dir;
    set_pin(dir, "123456");
    Pairing p = open_pairing(dir.path(), 24, fixed_clock());
    const auto r = p.pair("123456");
    REQUIRE(r.ok);
    std::string tampered = r.token;
    tampered.back() = tampered.back() == '0' ? '1' : '0';  // always a different MAC
    CHECK(!p.verify_token(tampered));
    CHECK(!p.verify_token("garbage"));
}

// "期限切れトークンを拒否する"
TEST_CASE(expired_token_rejected) {
    TempDir dir;
    set_pin(dir, "123456");
    std::int64_t now = 1'000'000;
    Pairing p = open_pairing(dir.path(), 1, [&now] { return now; });
    const auto r = p.pair("123456");
    REQUIRE(r.ok);
    now += 3'600'000 - 1;
    CHECK(p.verify_token(r.token));  // 1 ms before expiry
    now += 1;
    CHECK(!p.verify_token(r.token));  // expiry itself is already invalid (now < expiry)
}

TEST_CASE(lock_boundaries) {
    TempDir dir;
    set_pin(dir, "123456");
    std::int64_t now = 5'000'000;
    Pairing p = open_pairing(dir.path(), 24, [&now] { return now; });
    for (int i = 0; i < 4; ++i) CHECK(!p.pair("1111").ok);
    CHECK(p.pair("123456").ok);  // 4 failures + success: not locked, count reset
    for (int i = 0; i < 4; ++i) CHECK(!p.pair("1111").ok);
    CHECK(p.pair("1111").error == PairError::bad_pin);  // 5th failure locks
    now += 59'999;
    CHECK(p.pair("123456").error == PairError::locked);
    now += 1;
    CHECK(p.pair("123456").ok);  // lock over after exactly 60 s
}

TEST_CASE(attempts_while_locked_are_not_counted) {
    TempDir dir;
    set_pin(dir, "123456");
    std::int64_t now = 5'000'000;
    Pairing p = open_pairing(dir.path(), 24, [&now] { return now; });
    for (int i = 0; i < 5; ++i) p.pair("1111");
    for (int i = 0; i < 20; ++i) CHECK(p.pair("1111").error == PairError::locked);
    now += 60'000;
    for (int i = 0; i < 4; ++i) CHECK(p.pair("1111").error == PairError::bad_pin);
    CHECK(p.pair("123456").ok);
}

TEST_CASE(token_shapes_rejected) {
    TempDir dir;
    set_pin(dir, "123456");
    Pairing p = open_pairing(dir.path(), 24, fixed_clock());
    const auto r = p.pair("123456");
    REQUIRE(r.ok);
    const std::string mac = r.token.substr(r.token.find('.') + 1);
    CHECK(!p.verify_token(""));
    CHECK(!p.verify_token("."));
    CHECK(!p.verify_token("." + mac));                // empty payload (dot at 0)
    CHECK(!p.verify_token(r.token + "."));            // extra dot changes the MAC part
    CHECK(!p.verify_token("99999999999999." + mac));  // payload not covered by the MAC
    CHECK(!p.verify_token(r.token.substr(0, r.token.size() - 1)));
    std::string upper = r.token;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    CHECK(!p.verify_token(upper));  // hex is compared exactly
}

TEST_CASE(pin_format) {
    CHECK(is_valid_pin_format("1234"));
    CHECK(is_valid_pin_format("1234567890"));
    CHECK(!is_valid_pin_format("123"));
    CHECK(!is_valid_pin_format("12345678901"));
    CHECK(!is_valid_pin_format("12a4"));
    CHECK(!is_valid_pin_format(""));
    CHECK(!is_valid_pin_format("\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94"));  // full-width digits
    CHECK(!is_valid_pin_format(" 1234"));
}

TEST_CASE(store_pin_rejects_bad_format_and_never_stores_plain_pin) {
    TempDir dir;
    std::string error;
    CHECK(!store_pin(dir.path(), "12", error));
    CHECK_EQ(error, std::string("PIN は 4〜10 桁の数字で指定してください"));
    CHECK(!std::filesystem::exists(pairing_files(dir.path()).pairing));

    REQUIRE(store_pin(dir.path(), "987654", error));
    std::ifstream in(pairing_files(dir.path()).pairing);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(text.find("987654") == std::string::npos);
    const auto doc = nlohmann::json::parse(text);
    CHECK_EQ(doc["salt"].get<std::string>().size(), std::size_t{32});
    CHECK_EQ(doc["pinHash"].get<std::string>(), hash_pin("987654", doc["salt"].get<std::string>()));
    CHECK(text.rfind(R"({"salt":)", 0) == 0);  // same key order as JSON.stringify({ salt, pinHash })
}

TEST_CASE(hash_pin_known_vector) {
    // sha256("00112233445566778899aabbccddeeff:123456")
    CHECK_EQ(hash_pin("123456", "00112233445566778899aabbccddeeff"),
             manus::crypto::sha256_hex("00112233445566778899aabbccddeeff:123456"));
    CHECK_EQ(manus::crypto::sha256_hex("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(manus::crypto::hmac_sha256_hex("key", "The quick brown fox jumps over the lazy dog"),
             std::string("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"));
}

TEST_CASE(secret_created_once_and_reused) {
    TempDir dir;
    set_pin(dir, "123456");
    const auto first = load_pairing(dir.path());
    REQUIRE(first.pin.has_value());
    CHECK_EQ(first.secret.size(), std::size_t{32});
    const auto second = load_pairing(dir.path());
    CHECK(first.secret == second.secret);
}

TEST_CASE(secret_and_pairing_file_boundaries) {
    TempDir dir;
    set_pin(dir, "123456");
    const auto files = pairing_files(dir.path());
    dir.write("data/session-secret", std::string(31, 'k'));
    CHECK(!load_pairing(dir.path()).pin.has_value());  // too short
    dir.write("data/session-secret", std::string(32, 'k'));
    CHECK(load_pairing(dir.path()).pin.has_value());
    dir.write("data/session-secret", std::string(4096, 'k'));
    CHECK(load_pairing(dir.path()).pin.has_value());
    dir.write("data/session-secret", std::string(4097, 'k'));
    CHECK(!load_pairing(dir.path()).pin.has_value());  // too large to be ours
    dir.write("data/session-secret", std::string(32, 'k'));

    dir.write("data/pairing.json", "{\"salt\":1,\"pinHash\":\"x\"}");
    CHECK(!load_pairing(dir.path()).pin.has_value());
    dir.write("data/pairing.json", "not json");
    CHECK(!load_pairing(dir.path()).pin.has_value());
    dir.write("data/pairing.json", "{\"salt\":\"a\",\"pinHash\":\"b\"}" + std::string(64 * 1024, ' '));
    CHECK(!load_pairing(dir.path()).pin.has_value());  // over 64 KiB
    (void)files;
}

TEST_CASE(pair_accepts_any_string_like_node) {
    TempDir dir;
    set_pin(dir, "123456");
    Pairing p = open_pairing(dir.path(), 24, fixed_clock());
    CHECK(p.pair(std::string(4000, '9')).error == PairError::bad_pin);  // no format check at pair time
    CHECK(p.pair("").error == PairError::bad_pin);
}
