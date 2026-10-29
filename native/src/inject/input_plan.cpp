#include "inject/input_plan.hpp"

#include <cmath>
#include <optional>

#include <lorica/checked_size.hpp>
#include <lorica/input_budget.hpp>

#include "util/utf.hpp"

namespace manus::inject {
namespace {

// .NET Convert.ToInt32's OverflowException text, which the helper reported.
constexpr const char* kInt32Overflow = "Value was either too large or too small for an Int32.";

std::optional<std::int32_t> to_int32(double v) {
    if (!std::isfinite(v) || v < -2147483648.0 || v > 2147483647.0) return std::nullopt;
    return static_cast<std::int32_t>(v);
}

InputEvent key_event(std::uint16_t vk, std::uint16_t scan, std::uint32_t flags) {
    InputEvent e;
    e.type = InputEvent::Type::keyboard;
    e.vk = vk;
    e.scan = scan;
    e.flags = flags;
    return e;
}

InputEvent mouse_event(std::int32_t dx, std::int32_t dy, std::uint32_t data, std::uint32_t flags) {
    InputEvent e;
    e.type = InputEvent::Type::mouse;
    e.dx = dx;
    e.dy = dy;
    e.mouse_data = data;
    e.flags = flags;
    return e;
}

std::uint32_t key_flags(std::uint16_t vk, bool up) {
    std::uint32_t flags = up ? kKeyEventKeyUp : 0;
    if (is_extended_vk(vk)) flags |= kKeyEventExtendedKey;
    return flags;
}

InputPlan failure(std::string error) { return {false, {}, std::move(error)}; }

// Charges the number of records before the vector is allocated.
bool reserve_charged(std::vector<InputEvent>& events, std::size_t count, std::size_t max_events) {
    const auto bytes = lorica::checked_multiply(count, sizeof(InputEvent));
    const auto limit = lorica::checked_multiply(max_events, sizeof(InputEvent));
    if (!bytes || !limit) return false;
    lorica::input_budget budget({*limit, max_events, 1, 0});
    if (!budget.consume(*bytes, count, 1)) return false;
    events.reserve(count);
    return true;
}

constexpr const char* kTooMany = "too many input events for one message";

InputPlan plan_text(const TextCommand& c, std::size_t max_events) {
    const auto units = util::utf8_to_utf16(c.text);
    const auto count = lorica::checked_multiply(units.size(), 2);
    InputPlan plan;
    if (!count || !reserve_charged(plan.events, *count, max_events)) return failure(kTooMany);
    for (const std::uint16_t u : units) {
        if (u == '\r') continue;
        if (u == '\n' || u == '\t') {
            const std::uint16_t vk = u == '\n' ? kVkReturn : kVkTab;
            plan.events.push_back(key_event(vk, 0, 0));
            plan.events.push_back(key_event(vk, 0, kKeyEventKeyUp));
            continue;
        }
        // KEYEVENTF_UNICODE works per UTF-16 code unit; surrogate pairs combine correctly.
        plan.events.push_back(key_event(0, u, kKeyEventUnicode));
        plan.events.push_back(key_event(0, u, kKeyEventUnicode | kKeyEventKeyUp));
    }
    plan.ok = true;
    return plan;
}

InputPlan plan_keys(const KeyCommand& c, std::size_t max_events) {
    const auto count = lorica::checked_multiply(c.vks.size(), 2);
    InputPlan plan;
    if (!count || !reserve_charged(plan.events, *count, max_events)) return failure(kTooMany);
    if (c.action == KeyAction::down || c.action == KeyAction::press) {
        for (const auto vk : c.vks) plan.events.push_back(key_event(vk, 0, key_flags(vk, false)));
    }
    if (c.action == KeyAction::up || c.action == KeyAction::press) {
        for (auto it = c.vks.rbegin(); it != c.vks.rend(); ++it) plan.events.push_back(key_event(*it, 0, key_flags(*it, true)));
    }
    plan.ok = true;
    return plan;
}

InputPlan plan_move(const MouseMoveCommand& c) {
    const auto dx = to_int32(c.dx);
    const auto dy = to_int32(c.dy);
    if (!dx || !dy) return failure(kInt32Overflow);
    return {true, {mouse_event(*dx, *dy, 0, kMouseEventMove)}, {}};
}

InputPlan plan_button(const MouseButtonCommand& c) {
    std::uint32_t down = 0;
    std::uint32_t up = 0;
    switch (c.button) {
        case MouseButton::left: down = kMouseEventLeftDown; up = kMouseEventLeftUp; break;
        case MouseButton::right: down = kMouseEventRightDown; up = kMouseEventRightUp; break;
        case MouseButton::middle: down = kMouseEventMiddleDown; up = kMouseEventMiddleUp; break;
    }
    InputPlan plan{true, {}, {}};
    plan.events.reserve(2);
    if (c.action == ButtonAction::down || c.action == ButtonAction::click) plan.events.push_back(mouse_event(0, 0, 0, down));
    if (c.action == ButtonAction::up || c.action == ButtonAction::click) plan.events.push_back(mouse_event(0, 0, 0, up));
    return plan;
}

InputPlan plan_wheel(const WheelCommand& c) {
    const auto dx = to_int32(c.dx);
    const auto dy = to_int32(c.dy);
    if (!dx || !dy) return failure(kInt32Overflow);
    InputPlan plan{true, {}, {}};
    plan.events.reserve(2);
    // mouseData is a DWORD holding the signed delta (unchecked cast in the helper).
    if (*dy != 0) plan.events.push_back(mouse_event(0, 0, static_cast<std::uint32_t>(*dy), kMouseEventWheel));
    if (*dx != 0) plan.events.push_back(mouse_event(0, 0, static_cast<std::uint32_t>(*dx), kMouseEventHWheel));
    return plan;
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace

bool is_extended_vk(std::uint16_t vk) {
    switch (vk) {
        case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
        case 0x2D: case 0x2E: case 0x5B: case 0x5C: case 0x6F: case 0x90:
            return true;
        default:
            return false;
    }
}

InputPlan plan_input(const InjectCommand& command, std::size_t max_events) {
    return std::visit(overloaded{
                          [&](const TextCommand& c) { return plan_text(c, max_events); },
                          [&](const KeyCommand& c) { return plan_keys(c, max_events); },
                          [](const MouseMoveCommand& c) { return plan_move(c); },
                          [](const MouseButtonCommand& c) { return plan_button(c); },
                          [](const WheelCommand& c) { return plan_wheel(c); },
                      },
                      command);
}

}  // namespace manus::inject
