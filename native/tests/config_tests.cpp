// Mirrors src/config.ts (zod .strict() schema + MANUS_PORT / MANUS_BIND).
#include <filesystem>
#include <fstream>
#include <string>

#include "config/config.hpp"
#include "test_harness.hpp"

using manus::config::apply_env_overrides;
using manus::config::ManusConfig;
using manus::config::parse_config;
using json = nlohmann::json;

TEST_CASE(defaults_when_file_is_empty_object) {
    const auto r = parse_config(json::object());
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->port, std::uint16_t{5390});
    CHECK_EQ(r.config->bind, std::string("127.0.0.1"));  // loopback by default
    CHECK_EQ(r.config->token_ttl_hours, std::int64_t{24});
    CHECK_EQ(r.config->helper_dir, std::string("data/helper"));
}

TEST_CASE(accepts_every_known_key) {
    const auto r = parse_config(json::parse(
        R"({"port":6000,"bind":"0.0.0.0","foregroundPollMs":100,"tokenTtlHours":1,"helperDir":"x"})"));
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->port, std::uint16_t{6000});
    CHECK_EQ(r.config->bind, std::string("0.0.0.0"));
    CHECK_EQ(r.config->token_ttl_hours, std::int64_t{1});
}

TEST_CASE(strict_schema_rejects_unknown_keys_and_bad_types) {
    CHECK(!parse_config(json::parse(R"({"prot":1})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"port":"5390"})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"port":5390.5})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"bind":1})")).config.has_value());
    CHECK(!parse_config(json::parse(R"([])")).config.has_value());
    CHECK(!parse_config(json::parse(R"(null)")).config.has_value());
}

TEST_CASE(range_boundaries) {
    CHECK(parse_config(json::parse(R"({"port":1})")).config.has_value());
    CHECK(parse_config(json::parse(R"({"port":65535})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"port":0})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"port":65536})")).config.has_value());
    CHECK(parse_config(json::parse(R"({"port":5390.0})")).config.has_value());  // zod int() accepts 5390.0
    CHECK(parse_config(json::parse(R"({"foregroundPollMs":100})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"foregroundPollMs":99})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"tokenTtlHours":0})")).config.has_value());
    CHECK(parse_config(json::parse(R"({"tokenTtlHours":2000000000})")).config.has_value());
    CHECK(!parse_config(json::parse(R"({"tokenTtlHours":2000000001})")).config.has_value());
}

TEST_CASE(env_overrides) {
    ManusConfig base;
    auto r = apply_env_overrides(base, std::string("5391"), std::string("0.0.0.0"));
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->port, std::uint16_t{5391});
    CHECK_EQ(r.config->bind, std::string("0.0.0.0"));

    r = apply_env_overrides(base, std::string(""), std::string(""));  // empty = unset (JS falsy)
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->port, std::uint16_t{5390});
    CHECK_EQ(r.config->bind, std::string("127.0.0.1"));

    r = apply_env_overrides(base, std::string(" 5392 "), std::nullopt);
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->port, std::uint16_t{5392});

    for (const char* bad : {"abc", "0", "65536", "53.9", "-1", "0x1500", "999999"}) {
        CHECK(!apply_env_overrides(base, std::string(bad), std::nullopt).config.has_value());
    }
}

// --- file loading (config_loader.cpp) ---
#include "config/config_loader.hpp"
#include "temp_dir.hpp"

TEST_CASE(loader_missing_file_gives_defaults) {
    manus::test::TempDir dir;
    const auto r = manus::config::load_config(dir.path());
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->bind, std::string("127.0.0.1"));
}

TEST_CASE(loader_reads_file) {
    manus::test::TempDir dir;
    dir.write("manus.config.json", R"({"tokenTtlHours": 2})");
    const auto r = manus::config::load_config(dir.path());
    REQUIRE(r.config.has_value());
    CHECK_EQ(r.config->token_ttl_hours, std::int64_t{2});
}

TEST_CASE(loader_rejects_broken_and_oversized_files) {
    manus::test::TempDir dir;
    dir.write("manus.config.json", "{");
    CHECK(!manus::config::load_config(dir.path()).config.has_value());
    // exactly 64 KiB of whitespace around {} is accepted, one byte more is refused
    std::string doc = "{}";
    doc.append(64 * 1024 - doc.size(), ' ');
    dir.write("manus.config.json", doc);
    CHECK(manus::config::load_config(dir.path()).config.has_value());
    doc.push_back(' ');
    dir.write("manus.config.json", doc);
    CHECK(!manus::config::load_config(dir.path()).config.has_value());
    // deeper than 16 levels
    dir.write("manus.config.json", R"({"port":[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]})");
    CHECK(!manus::config::load_config(dir.path()).config.has_value());
}
