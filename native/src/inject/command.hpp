#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace manus::inject {

// What one client message asks to inject (the HelperCommand of the Node
// implementation, now handed to the input planner instead of a helper process).

struct TextCommand {
    std::string text;  // UTF-8
};

enum class KeyAction { press, down, up };

// vks: modifiers first, target key last. press = down in order, up in reverse.
struct KeyCommand {
    std::vector<std::uint16_t> vks;
    KeyAction action = KeyAction::press;
};

// Relative move; values are already truncated toward zero (Math.trunc).
struct MouseMoveCommand {
    double dx = 0;
    double dy = 0;
};

enum class MouseButton { left, right, middle };
enum class ButtonAction { down, up, click };

struct MouseButtonCommand {
    MouseButton button = MouseButton::left;
    ButtonAction action = ButtonAction::click;
};

// Wheel deltas (120 = one notch); truncated toward zero.
struct WheelCommand {
    double dx = 0;
    double dy = 0;
};

using InjectCommand = std::variant<TextCommand, KeyCommand, MouseMoveCommand, MouseButtonCommand, WheelCommand>;

std::string_view to_string(KeyAction action);
std::string_view to_string(MouseButton button);
std::string_view to_string(ButtonAction action);

// The command as the Node server serialised it for the helper (without the
// request id): {"op":"key","vks":[17,67],"action":"press"} and so on. Used to
// compare with the golden records of the Node implementation.
std::string command_to_json(const InjectCommand& command);

}  // namespace manus::inject
