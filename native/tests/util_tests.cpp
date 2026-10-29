#include <string>

#include "test_harness.hpp"
#include "util/bounded_json.hpp"
#include "util/js_number.hpp"
#include "util/utf.hpp"

using manus::util::JsonLimits;
using manus::util::js_number_to_string;
using manus::util::parse_bounded_json;

TEST_CASE(bounded_json_parses_plain_documents) {
    const auto r = parse_bounded_json(R"({"a":[1,2,{"b":"c"}],"d":null})", {1024, 64, 8});
    REQUIRE(r.value.has_value());
    CHECK_EQ((*r.value)["a"][2]["b"].get<std::string>(), std::string("c"));
    CHECK(r.error.empty());
}

TEST_CASE(bounded_json_duplicate_keys_keep_last_value_like_json_parse) {
    const auto r = parse_bounded_json(R"({"pin":"1","pin":"2"})", {1024, 64, 8});
    REQUIRE(r.value.has_value());
    CHECK_EQ((*r.value)["pin"].get<std::string>(), std::string("2"));
}

TEST_CASE(bounded_json_byte_limit_exact_and_plus_one) {
    const std::string doc = R"({"k":"0123456789"})";  // 18 bytes
    CHECK(parse_bounded_json(doc, {doc.size(), 64, 8}).value.has_value());
    CHECK(!parse_bounded_json(doc, {doc.size() - 1, 64, 8}).value.has_value());
}

TEST_CASE(bounded_json_depth_limit_exact_and_plus_one) {
    CHECK(parse_bounded_json("[[[1]]]", {1024, 64, 3}).value.has_value());
    const auto deep = parse_bounded_json("[[[[1]]]]", {1024, 64, 3});
    CHECK(!deep.value.has_value());
    CHECK(!deep.error.empty());
}

TEST_CASE(bounded_json_element_limit) {
    // [1,2,3] = 1 array + 3 numbers = 4 elements
    CHECK(parse_bounded_json("[1,2,3]", {1024, 4, 8}).value.has_value());
    CHECK(!parse_bounded_json("[1,2,3]", {1024, 3, 8}).value.has_value());
}

TEST_CASE(bounded_json_huge_nesting_is_refused_without_recursion) {
    const std::string doc(200000, '[');
    const auto r = parse_bounded_json(doc, {1024 * 1024, 1024 * 1024, 64});
    CHECK(!r.value.has_value());
}

TEST_CASE(bounded_json_broken_input) {
    CHECK(!parse_bounded_json("", {1024, 64, 8}).value.has_value());
    CHECK(!parse_bounded_json("{\"a\":", {1024, 64, 8}).value.has_value());
    CHECK(!parse_bounded_json("{'a':1}", {1024, 64, 8}).value.has_value());
    CHECK(!parse_bounded_json("[1] // c", {1024, 64, 8}).value.has_value());
    CHECK(!parse_bounded_json("\"\xff\"", {1024, 64, 8}).value.has_value());       // invalid UTF-8
    CHECK(!parse_bounded_json("\"\\ud800\"", {1024, 64, 8}).value.has_value());    // lone surrogate
    CHECK(!parse_bounded_json("1e400", {1024, 64, 8}).value.has_value());          // overflow
}

TEST_CASE(js_number_matches_json_stringify) {
    CHECK_EQ(js_number_to_string(0.0), std::string("0"));
    CHECK_EQ(js_number_to_string(-0.0), std::string("0"));
    CHECK_EQ(js_number_to_string(1), std::string("1"));
    CHECK_EQ(js_number_to_string(-42), std::string("-42"));
    CHECK_EQ(js_number_to_string(1.5), std::string("1.5"));
    CHECK_EQ(js_number_to_string(0.1 + 0.2), std::string("0.30000000000000004"));
    CHECK_EQ(js_number_to_string(1e21), std::string("1e+21"));
    CHECK_EQ(js_number_to_string(123456789012345680000.0), std::string("123456789012345680000"));
    CHECK_EQ(js_number_to_string(1e-7), std::string("1e-7"));
    CHECK_EQ(js_number_to_string(5e-7), std::string("5e-7"));
    CHECK_EQ(js_number_to_string(0.000001), std::string("0.000001"));
    CHECK_EQ(js_number_to_string(1.5e300), std::string("1.5e+300"));
    CHECK_EQ(js_number_to_string(1730000000123.0), std::string("1730000000123"));
    CHECK_EQ(js_number_to_string(9007199254740993.0), std::string("9007199254740992"));
}

TEST_CASE(utf16_length_counts_like_js_string_length) {
    CHECK_EQ(manus::util::utf16_length("abc"), std::size_t{3});
    CHECK_EQ(manus::util::utf16_length("\xE3\x81\x82"), std::size_t{1});          // あ
    CHECK_EQ(manus::util::utf16_length("\xF0\x9F\x98\x80"), std::size_t{2});      // 😀 (surrogate pair)
}

TEST_CASE(utf8_to_utf16_splits_supplementary_characters) {
    const auto units = manus::util::utf8_to_utf16("A\xF0\x9F\x98\x80");
    REQUIRE(units.size() == 3);
    CHECK_EQ(units[0], std::uint16_t{0x41});
    CHECK_EQ(units[1], std::uint16_t{0xD83D});
    CHECK_EQ(units[2], std::uint16_t{0xDE00});
}

TEST_CASE(utf8_validation_rejects_overlong_and_surrogates) {
    CHECK(manus::util::is_valid_utf8("\xE3\x81\x82"));
    CHECK(!manus::util::is_valid_utf8("\xC0\x80"));          // overlong NUL
    CHECK(!manus::util::is_valid_utf8("\xED\xA0\x80"));      // U+D800
    CHECK(!manus::util::is_valid_utf8("\xF4\x90\x80\x80"));  // > U+10FFFF
    CHECK(!manus::util::is_valid_utf8("\xE3\x81"));          // truncated
}
