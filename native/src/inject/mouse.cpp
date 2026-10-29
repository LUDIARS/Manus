#include "inject/mouse.hpp"

#include <cmath>

namespace manus::inject {

MouseMoveCommand move_command(double dx, double dy) { return {std::trunc(dx), std::trunc(dy)}; }

MouseButtonCommand button_command(MouseButton button, ButtonAction action) { return {button, action}; }

WheelCommand wheel_command(double dx, double dy) { return {std::trunc(dx), std::trunc(dy)}; }

}  // namespace manus::inject
