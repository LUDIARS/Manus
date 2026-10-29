// The INPUT records helper/ManusInputHelper.cs produced for each command.
#include <string>

#include "fake_input_sink.hpp"
#include "inject/input_plan.hpp"
#include "inject/keyboard.hpp"
#include "inject/mouse.hpp"
#include "test_harness.hpp"

using namespace manus::inject;
using Type = InputEvent::Type;

namespace {
InputEvent key(std::uint16_t vk, std::uint32_t flags) { return {Type::keyboard, vk, 0, 0, 0, 0, flags}; }
InputEvent mouse(std::int32_t dx, std::int32_t dy, std::uint32_t data, std::uint32_t flags) {
    return {Type::mouse, 0, 0, dx, dy, data, flags};
}
}  // namespace

TEST_CASE(key_press_downs_in_order_ups_in_reverse) {
    const auto plan = plan_input(*key_command("s", {Modifier::ctrl, Modifier::shift}).command);
    REQUIRE(plan.ok);
    const std::vector<InputEvent> expected = {
        key(0x11, 0), key(0x10, 0), key(0x53, 0),
        key(0x53, kKeyEventKeyUp), key(0x10, kKeyEventKeyUp), key(0x11, kKeyEventKeyUp),
    };
    CHECK(plan.events == expected);
}

TEST_CASE(key_down_and_up_only) {
    auto down = plan_input(*key_command("a", {Modifier::shift}, KeyAction::down).command);
    REQUIRE(down.ok);
    CHECK(down.events == (std::vector<InputEvent>{key(0x10, 0), key(0x41, 0)}));
    auto up = plan_input(*key_command("a", {Modifier::shift}, KeyAction::up).command);
    REQUIRE(up.ok);
    CHECK(up.events == (std::vector<InputEvent>{key(0x41, kKeyEventKeyUp), key(0x10, kKeyEventKeyUp)}));
}

TEST_CASE(extended_keys_get_the_extended_flag) {
    for (const char* name : {"pgup", "pgdn", "end", "home", "left", "up", "right", "down", "ins", "del", "numdiv", "numlock"}) {
        const auto plan = plan_input(*key_command(name).command);
        REQUIRE(plan.ok);
        REQUIRE(plan.events.size() == 2);
        CHECK_EQ(plan.events[0].flags, kKeyEventExtendedKey);
        CHECK_EQ(plan.events[1].flags, kKeyEventExtendedKey | kKeyEventKeyUp);
    }
    // win (0x5B) is extended too, as a modifier
    const auto win_e = plan_input(*key_command("e", {Modifier::win}).command);
    CHECK_EQ(win_e.events[0].flags, kKeyEventExtendedKey);
    CHECK_EQ(win_e.events[1].flags, std::uint32_t{0});
    // ordinary keys are not
    for (const char* name : {"enter", "a", "f5", "num5", "henkan", "space"}) {
        CHECK_EQ(plan_input(*key_command(name).command).events[0].flags, std::uint32_t{0});
    }
}

TEST_CASE(mouse_move_single_event) {
    const auto plan = plan_input(move_command(-5.7, 12.2));
    REQUIRE(plan.ok);
    CHECK(plan.events == (std::vector<InputEvent>{mouse(-5, 12, 0, kMouseEventMove)}));
    CHECK(plan_input(move_command(0, 0)).events.size() == 1);  // a zero move is still sent
}

TEST_CASE(mouse_buttons) {
    auto plan = plan_input(button_command(MouseButton::left, ButtonAction::click));
    CHECK(plan.events == (std::vector<InputEvent>{mouse(0, 0, 0, kMouseEventLeftDown), mouse(0, 0, 0, kMouseEventLeftUp)}));
    plan = plan_input(button_command(MouseButton::right, ButtonAction::down));
    CHECK(plan.events == (std::vector<InputEvent>{mouse(0, 0, 0, kMouseEventRightDown)}));
    plan = plan_input(button_command(MouseButton::middle, ButtonAction::up));
    CHECK(plan.events == (std::vector<InputEvent>{mouse(0, 0, 0, kMouseEventMiddleUp)}));
}

TEST_CASE(wheel_vertical_then_horizontal_and_zero_is_nothing) {
    auto plan = plan_input(wheel_command(30, -120));
    REQUIRE(plan.ok);
    CHECK(plan.events == (std::vector<InputEvent>{mouse(0, 0, static_cast<std::uint32_t>(-120), kMouseEventWheel),
                                                  mouse(0, 0, 30, kMouseEventHWheel)}));
    plan = plan_input(wheel_command(0, 0));
    CHECK(plan.ok);
    CHECK(plan.events.empty());
}

TEST_CASE(int32_range_boundaries) {
    CHECK(plan_input(move_command(2147483647, -2147483648.0)).ok);
    const auto over = plan_input(move_command(2147483648.0, 0));
    CHECK(!over.ok);
    CHECK_EQ(over.error, std::string("Value was either too large or too small for an Int32."));
    CHECK(!plan_input(move_command(0, -2147483649.0)).ok);
    CHECK(!plan_input(wheel_command(1e300, 0)).ok);
    CHECK(plan_input(wheel_command(-2147483648.0, 2147483647)).ok);
}

TEST_CASE(event_count_limit_exact_and_plus_one) {
    // 4 characters = 8 records
    CHECK(plan_input(text_command("abcd"), 8).ok);
    const auto over = plan_input(text_command("abcde"), 8);
    CHECK(!over.ok);
    CHECK(over.events.empty());
    // the largest text the protocol allows (4096 units) fits the default limit
    CHECK(plan_input(text_command(std::string(4096, 'x'))).ok);
}

TEST_CASE(fake_sink_stands_in_for_send_input) {
    manus::test::FakeInputSink sink;
    const auto plan = plan_input(text_command("hi"));
    CHECK(sink.send(plan.events).ok);
    REQUIRE(sink.calls.size() == 1);
    CHECK_EQ(sink.calls[0].size(), std::size_t{4});
    sink.up = false;
    CHECK_EQ(sink.send(plan.events).error, std::string("helper_down"));
}
