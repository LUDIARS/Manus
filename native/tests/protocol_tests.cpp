// src/ws/protocol.ts (zod) acceptance rules; spec/test/test-plan.md "ws/protocol":
// invalid type / out-of-range values / extra fields.
#include <string>

#include "test_harness.hpp"
#include "ws/protocol.hpp"

using namespace manus::ws;

namespace {
bool accepts(const std::string& raw) { return parse_client_message(raw).message.has_value(); }
}  // namespace

TEST_CASE(accepts_every_message_type) {
    CHECK(accepts(R"({"type":"text","text":"あいう"})"));
    CHECK(accepts(R"({"type":"text","text":"","seq":1})"));
    CHECK(accepts(R"({"type":"key","key":"enter"})"));
    CHECK(accepts(R"({"type":"key","key":"c","modifiers":["ctrl","shift","alt","win"],"action":"down","seq":2})"));
    CHECK(accepts(R"({"type":"key","key":"c","modifiers":[]})"));
    CHECK(accepts(R"({"type":"mouse.move","dx":1.5,"dy":-3})"));
    CHECK(accepts(R"({"type":"mouse.button","button":"middle","action":"click"})"));
    CHECK(accepts(R"({"type":"mouse.wheel"})"));
    CHECK(accepts(R"({"type":"mouse.wheel","dy":-120,"seq":9})"));
    CHECK(accepts(R"({"type":"ping"})"));
    CHECK(accepts(R"({"type":"ping","seq":0})"));
}

TEST_CASE(extra_fields_are_ignored) {
    const auto r = parse_client_message(R"({"type":"key","key":"a","extra":{"deep":[1,2]},"seq":3})");
    REQUIRE(r.message.has_value());
    CHECK_EQ(std::get<KeyMessage>(*r.message).key, std::string("a"));
    CHECK_EQ(seq_of(*r.message).value_or(-1), 3.0);
}

TEST_CASE(rejects_bad_type_and_shape) {
    for (const char* raw : {"", "not json", "123", "null", "[]", R"("text")", R"({})", R"({"type":1})",
                            R"({"type":"Text","text":"a"})", R"({"type":"pad.button"})", R"({"type":"shortcut"})"}) {
        CHECK(!accepts(raw));
    }
}

TEST_CASE(rejects_wrong_field_types) {
    for (const char* raw : {
             R"({"type":"text"})",
             R"({"type":"text","text":1})",
             R"({"type":"text","text":"a","seq":"1"})",
             R"({"type":"text","text":"a","seq":null})",
             R"({"type":"key"})",
             R"({"type":"key","key":"a","modifiers":"ctrl"})",
             R"({"type":"key","key":"a","modifiers":["meta"]})",
             R"({"type":"key","key":"a","modifiers":[null]})",
             R"({"type":"key","key":"a","modifiers":null})",
             R"({"type":"key","key":"a","action":"click"})",
             R"({"type":"mouse.move","dx":1})",
             R"({"type":"mouse.move","dx":"1","dy":1})",
             R"({"type":"mouse.move","dx":true,"dy":1})",
             R"({"type":"mouse.button","button":"left"})",
             R"({"type":"mouse.button","button":"x1","action":"click"})",
             R"({"type":"mouse.button","button":"left","action":"press"})",
             R"({"type":"mouse.wheel","dy":null})",
             R"({"type":"ping","seq":[1]})",
         }) {
        CHECK(!accepts(raw));
    }
}

TEST_CASE(text_length_counts_utf16_units_exact_and_plus_one) {
    CHECK(accepts(R"({"type":"text","text":")" + std::string(4096, 'a') + R"("})"));
    CHECK(!accepts(R"({"type":"text","text":")" + std::string(4097, 'a') + R"("})"));
    // 2048 emoji = 4096 UTF-16 units (accepted); one more BMP char is 4097
    std::string emoji;
    for (int i = 0; i < 2048; ++i) emoji += "\xF0\x9F\x98\x80";
    CHECK(accepts(R"({"type":"text","text":")" + emoji + R"("})"));
    CHECK(!accepts(R"({"type":"text","text":")" + emoji + R"(a"})"));
    // 4096 Japanese characters (3 bytes each in UTF-8) are still 4096 units
    std::string kana;
    for (int i = 0; i < 4096; ++i) kana += "\xE3\x81\x82";
    CHECK(accepts(R"({"type":"text","text":")" + kana + R"("})"));
}

TEST_CASE(key_length_bounds) {
    CHECK(!accepts(R"({"type":"key","key":""})"));
    CHECK(accepts(R"({"type":"key","key":")" + std::string(16, 'k') + R"("})"));
    CHECK(!accepts(R"({"type":"key","key":")" + std::string(17, 'k') + R"("})"));
}

TEST_CASE(json_budget_limits) {
    // 64 KiB raw limit (exact accepted, +1 refused)
    std::string base = R"({"type":"ping","pad":")";
    const std::string tail = R"("})";
    std::string exact = base + std::string(kMessageLimits.max_bytes - base.size() - tail.size(), 'p') + tail;
    CHECK_EQ(exact.size(), kMessageLimits.max_bytes);
    CHECK(accepts(exact));
    std::string over = base + std::string(kMessageLimits.max_bytes - base.size() - tail.size() + 1, 'p') + tail;
    CHECK(!accepts(over));
    // depth: 8 levels accepted, 9 refused
    CHECK(accepts(R"({"type":"ping","x":[[[[[[[1]]]]]]]})"));
    CHECK(!accepts(R"({"type":"ping","x":[[[[[[[[1]]]]]]]]})"));
    // elements: a modifiers list of 1000 is within 1024 values, 1100 is not
    auto mods = [](int n) {
        std::string s = R"({"type":"key","key":"a","modifiers":[)";
        for (int i = 0; i < n; ++i) s += (i ? ",\"ctrl\"" : "\"ctrl\"");
        return s + "]}";
    };
    CHECK(accepts(mods(1000)));
    CHECK(!accepts(mods(1100)));
}

TEST_CASE(broken_and_hostile_json) {
    CHECK(!accepts(R"({"type":"text","text":"\ud800"})"));  // lone surrogate
    CHECK(!accepts("{\"type\":\"text\",\"text\":\"\xff\"}"));  // invalid UTF-8
    CHECK(!accepts(R"({"type":"mouse.move","dx":1e400,"dy":0})"));
    CHECK(!accepts(std::string(100000, '[')));
    CHECK(!accepts(R"({"type":"ping"} trailing)"));
}

TEST_CASE(duplicate_keys_keep_last_like_json_parse) {
    const auto r = parse_client_message(R"({"type":"ping","type":"text","text":"x"})");
    REQUIRE(r.message.has_value());
    CHECK(std::holds_alternative<TextMessage>(*r.message));
}
