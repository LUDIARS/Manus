#include "inject/command.hpp"

#include <nlohmann/json.hpp>

#include "util/js_number.hpp"

namespace manus::inject {
namespace {

std::string quote(std::string_view s) {
    return nlohmann::json(std::string(s)).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace

std::string_view to_string(KeyAction action) {
    switch (action) {
        case KeyAction::press: return "press";
        case KeyAction::down: return "down";
        case KeyAction::up: return "up";
    }
    return "press";
}

std::string_view to_string(MouseButton button) {
    switch (button) {
        case MouseButton::left: return "left";
        case MouseButton::right: return "right";
        case MouseButton::middle: return "middle";
    }
    return "left";
}

std::string_view to_string(ButtonAction action) {
    switch (action) {
        case ButtonAction::down: return "down";
        case ButtonAction::up: return "up";
        case ButtonAction::click: return "click";
    }
    return "click";
}

std::string command_to_json(const InjectCommand& command) {
    using util::js_number_to_string;
    return std::visit(
        overloaded{
            [](const TextCommand& c) { return R"({"op":"text","text":)" + quote(c.text) + "}"; },
            [](const KeyCommand& c) {
                std::string vks;
                for (std::size_t i = 0; i < c.vks.size(); ++i) {
                    if (i > 0) vks += ',';
                    vks += std::to_string(c.vks[i]);
                }
                return R"({"op":"key","vks":[)" + vks + R"(],"action":)" + quote(to_string(c.action)) + "}";
            },
            [](const MouseMoveCommand& c) {
                return R"({"op":"mouse_move","dx":)" + js_number_to_string(c.dx) + R"(,"dy":)" +
                       js_number_to_string(c.dy) + "}";
            },
            [](const MouseButtonCommand& c) {
                return R"({"op":"mouse_button","button":)" + quote(to_string(c.button)) + R"(,"action":)" +
                       quote(to_string(c.action)) + "}";
            },
            [](const WheelCommand& c) {
                return R"({"op":"wheel","dx":)" + js_number_to_string(c.dx) + R"(,"dy":)" +
                       js_number_to_string(c.dy) + "}";
            },
        },
        command);
}

}  // namespace manus::inject
