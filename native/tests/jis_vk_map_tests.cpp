// Ports src/inject/jis-vk-map.test.ts.
#include <map>
#include <string>

#include "inject/jis_vk_map.hpp"
#include "inject/keyboard.hpp"
#include "test_harness.hpp"

using namespace manus::inject;

// "英字は ASCII 大文字の VK"
TEST_CASE(letters_are_ascii_uppercase_vk) {
    CHECK_EQ(find_vk("a").value_or(0), std::uint16_t{0x41});
    CHECK_EQ(find_vk("z").value_or(0), std::uint16_t{0x5a});
}

// "矢印・ファンクション・JIS 固有キーを持つ"
TEST_CASE(arrows_function_and_jis_keys) {
    CHECK_EQ(find_vk("up").value_or(0), std::uint16_t{0x26});
    CHECK_EQ(find_vk("f12").value_or(0), std::uint16_t{0x7b});
    CHECK_EQ(find_vk("henkan").value_or(0), std::uint16_t{0x1c});
    CHECK_EQ(find_vk("muhenkan").value_or(0), std::uint16_t{0x1d});
    CHECK_EQ(find_vk("kana").value_or(0), std::uint16_t{0x15});
}

// "VK が重複しない (テンキーと本体キーは別コード)" — capslock/eisu share 0x14 by design.
TEST_CASE(no_duplicate_vks_except_capslock_eisu) {
    std::map<std::uint16_t, std::string> seen;
    std::string dups;
    for (const auto& [name, vk] : jis_vk_table()) {
        const auto it = seen.find(vk);
        if (it != seen.end() && !(vk == 0x14 && it->second != name)) dups += it->second + "/" + std::string(name) + " ";
        seen[vk] = std::string(name);
    }
    CHECK_EQ(dups, std::string());
}

// "未知キーは投げる" -> unknown names give no VK (and key_command reports the vkOf message).
TEST_CASE(unknown_key_has_no_vk) {
    CHECK(!find_vk("banana").has_value());
    CHECK_EQ(key_command("banana").error, std::string("unknown key name: banana"));
}

TEST_CASE(inherited_js_property_names_are_not_keys) {
    // In Node, JIS_VK_MAP["constructor"] found Object.prototype members.
    for (const char* name : {"constructor", "__proto__", "toString", "valueOf", "hasOwnProperty", ""}) {
        CHECK(!find_vk(name).has_value());
    }
}

TEST_CASE(table_size_and_case_sensitivity) {
    CHECK_EQ(jis_vk_table().size(), std::size_t{101});
    CHECK(!find_vk("A").has_value());
    CHECK(!find_vk("Enter").has_value());
}

TEST_CASE(modifier_vks) {
    CHECK_EQ(modifier_vk(Modifier::ctrl), std::uint16_t{0x11});
    CHECK_EQ(modifier_vk(Modifier::shift), std::uint16_t{0x10});
    CHECK_EQ(modifier_vk(Modifier::alt), std::uint16_t{0x12});
    CHECK_EQ(modifier_vk(Modifier::win), std::uint16_t{0x5b});
    CHECK(!parse_modifier("meta").has_value());
    CHECK(!parse_modifier("Ctrl").has_value());
}
