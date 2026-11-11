// Windows only: the SendInput adapter with SendInput replaced by a fake that
// records the INPUT array (no input reaches the desktop).
#include <climits>
#include <string>
#include <vector>

#include "inject/input_plan.hpp"
#include "inject/keyboard.hpp"
#include "inject/mouse.hpp"
#include "inject/win32_input_sink.hpp"
#include "test_harness.hpp"

using namespace manus::inject;

namespace {

std::vector<INPUT> g_inputs;
int g_size = 0;
UINT g_accept = UINT_MAX;  // how many records the fake claims to have injected

UINT WINAPI fake_send_input(UINT count, LPINPUT inputs, int size) {
    g_inputs.assign(inputs, inputs + count);
    g_size = size;
    if (g_accept != UINT_MAX) {
        ::SetLastError(5);
        return g_accept;
    }
    return count;
}

void reset() {
    g_inputs.clear();
    g_size = 0;
    g_accept = UINT_MAX;
}

}  // namespace

TEST_CASE(text_becomes_unicode_keyboard_inputs) {
    reset();
    auto sink = make_win32_input_sink(&fake_send_input);
    CHECK(sink->available());
    const auto plan = plan_input(text_command("\xE3\x81\x82"));
    CHECK(sink->send(plan.events).ok);
    REQUIRE(g_inputs.size() == 2);
    CHECK_EQ(g_size, static_cast<int>(sizeof(INPUT)));
    CHECK_EQ(g_inputs[0].type, static_cast<DWORD>(INPUT_KEYBOARD));
    CHECK_EQ(g_inputs[0].ki.wVk, static_cast<WORD>(0));
    CHECK_EQ(g_inputs[0].ki.wScan, static_cast<WORD>(0x3042));
    CHECK_EQ(g_inputs[0].ki.dwFlags, static_cast<DWORD>(KEYEVENTF_UNICODE));
    CHECK_EQ(g_inputs[1].ki.dwFlags, static_cast<DWORD>(KEYEVENTF_UNICODE | KEYEVENTF_KEYUP));
}

TEST_CASE(keys_and_extended_flag) {
    reset();
    auto sink = make_win32_input_sink(&fake_send_input);
    CHECK(sink->send(plan_input(*key_command("left", {Modifier::ctrl}).command).events).ok);
    REQUIRE(g_inputs.size() == 4);
    CHECK_EQ(g_inputs[0].ki.wVk, static_cast<WORD>(VK_CONTROL));
    CHECK_EQ(g_inputs[1].ki.wVk, static_cast<WORD>(VK_LEFT));
    CHECK_EQ(g_inputs[1].ki.dwFlags, static_cast<DWORD>(KEYEVENTF_EXTENDEDKEY));
    CHECK_EQ(g_inputs[2].ki.dwFlags, static_cast<DWORD>(KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP));
    CHECK_EQ(g_inputs[3].ki.wVk, static_cast<WORD>(VK_CONTROL));
}

TEST_CASE(mouse_and_wheel_records) {
    reset();
    auto sink = make_win32_input_sink(&fake_send_input);
    CHECK(sink->send(plan_input(move_command(-3, 4)).events).ok);
    REQUIRE(g_inputs.size() == 1);
    CHECK_EQ(g_inputs[0].type, static_cast<DWORD>(INPUT_MOUSE));
    CHECK_EQ(g_inputs[0].mi.dx, static_cast<LONG>(-3));
    CHECK_EQ(g_inputs[0].mi.dy, static_cast<LONG>(4));
    CHECK_EQ(g_inputs[0].mi.dwFlags, static_cast<DWORD>(MOUSEEVENTF_MOVE));
    CHECK(sink->send(plan_input(wheel_command(0, -120)).events).ok);
    REQUIRE(g_inputs.size() == 1);
    CHECK_EQ(static_cast<int>(g_inputs[0].mi.mouseData), -120);
    CHECK_EQ(g_inputs[0].mi.dwFlags, static_cast<DWORD>(MOUSEEVENTF_WHEEL));
}

TEST_CASE(short_count_is_reported_like_the_helper) {
    reset();
    auto sink = make_win32_input_sink(&fake_send_input);
    g_accept = 1;
    const auto r = sink->send(plan_input(text_command("ab")).events);
    CHECK(!r.ok);
    CHECK_EQ(r.error, std::string("SendInput injected 1/4 (err=5)"));
}

TEST_CASE(empty_plan_does_not_call_send_input) {
    reset();
    auto sink = make_win32_input_sink(&fake_send_input);
    CHECK(sink->send({}).ok);
    CHECK(g_inputs.empty());
    CHECK_EQ(g_size, 0);
}
