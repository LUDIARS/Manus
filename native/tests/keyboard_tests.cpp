// Ports src/inject/keyboard.test.ts, plus the text cases of spec/test/test-plan.md
// (\n -> Enter, \t -> Tab, surrogate pairs are not split) on the input plan.
#include <string>

#include "inject/command.hpp"
#include "inject/input_plan.hpp"
#include "inject/keyboard.hpp"
#include "inject/mouse.hpp"
#include "test_harness.hpp"

using namespace manus::inject;

// textCommand "文字列をそのまま text op にする"
TEST_CASE(text_command_keeps_the_string) {
    CHECK_EQ(command_to_json(text_command("\xE3\x81\x82" "A1")), std::string(R"({"op":"text","text":"あA1"})"));
}

// keyCommand "修飾キーを先頭に、対象キーを末尾に並べる"
TEST_CASE(modifiers_first_key_last) {
    const auto r = key_command("c", {Modifier::ctrl});
    REQUIRE(r.command.has_value());
    CHECK_EQ(command_to_json(*r.command), std::string(R"({"op":"key","vks":[17,67],"action":"press"})"));
}

// "複数修飾は指定順で先頭に積む"
TEST_CASE(multiple_modifiers_keep_their_order) {
    const auto r = key_command("s", {Modifier::ctrl, Modifier::shift});
    REQUIRE(r.command.has_value());
    CHECK_EQ(command_to_json(*r.command), std::string(R"({"op":"key","vks":[17,16,83],"action":"press"})"));
}

// "action を透過する"
TEST_CASE(action_passes_through) {
    const auto r = key_command("enter", {}, KeyAction::down);
    REQUIRE(r.command.has_value());
    CHECK(r.command->action == KeyAction::down);
}

// "未知キー名は投げる"
TEST_CASE(unknown_key_name_is_an_error) {
    const auto r = key_command("nope");
    CHECK(!r.command.has_value());
    CHECK(r.error.find("unknown key") != std::string::npos);
}

TEST_CASE(mouse_commands_truncate_toward_zero) {
    CHECK_EQ(command_to_json(move_command(1.9, -1.9)), std::string(R"({"op":"mouse_move","dx":1,"dy":-1})"));
    CHECK_EQ(command_to_json(move_command(-0.5, 0.5)), std::string(R"({"op":"mouse_move","dx":0,"dy":0})"));
    CHECK_EQ(command_to_json(wheel_command(0, -120.7)), std::string(R"({"op":"wheel","dx":0,"dy":-120})"));
    CHECK_EQ(command_to_json(button_command(MouseButton::middle, ButtonAction::click)),
             std::string(R"({"op":"mouse_button","button":"middle","action":"click"})"));
}

// test-plan: text -> helper op: \n -> enter, \t -> tab, surrogate pairs not split
TEST_CASE(text_plan_newline_tab_and_surrogates) {
    const auto plan = plan_input(text_command("a\r\n\t\xF0\x9F\x98\x80"));
    REQUIRE(plan.ok);
    const std::vector<InputEvent> expected = {
        {InputEvent::Type::keyboard, 0, 'a', 0, 0, 0, kKeyEventUnicode},
        {InputEvent::Type::keyboard, 0, 'a', 0, 0, 0, kKeyEventUnicode | kKeyEventKeyUp},
        // \r is dropped, \n is an Enter press
        {InputEvent::Type::keyboard, kVkReturn, 0, 0, 0, 0, 0},
        {InputEvent::Type::keyboard, kVkReturn, 0, 0, 0, 0, kKeyEventKeyUp},
        {InputEvent::Type::keyboard, kVkTab, 0, 0, 0, 0, 0},
        {InputEvent::Type::keyboard, kVkTab, 0, 0, 0, 0, kKeyEventKeyUp},
        // 😀 = D83D DE00, each unit down/up, high surrogate first
        {InputEvent::Type::keyboard, 0, 0xD83D, 0, 0, 0, kKeyEventUnicode},
        {InputEvent::Type::keyboard, 0, 0xD83D, 0, 0, 0, kKeyEventUnicode | kKeyEventKeyUp},
        {InputEvent::Type::keyboard, 0, 0xDE00, 0, 0, 0, kKeyEventUnicode},
        {InputEvent::Type::keyboard, 0, 0xDE00, 0, 0, 0, kKeyEventUnicode | kKeyEventKeyUp},
    };
    CHECK(plan.events == expected);
}

TEST_CASE(text_plan_japanese_is_unicode_injection) {
    const auto plan = plan_input(text_command("\xE6\x97\xA5\xE6\x9C\xAC"));  // 日本
    REQUIRE(plan.ok);
    REQUIRE(plan.events.size() == 4);
    CHECK_EQ(plan.events[0].scan, std::uint16_t{0x65E5});
    CHECK_EQ(plan.events[2].scan, std::uint16_t{0x672C});
    CHECK_EQ(plan.events[0].vk, std::uint16_t{0});
}
