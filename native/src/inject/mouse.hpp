#pragma once

#include "inject/command.hpp"

namespace manus::inject {

// src/inject/mouse.ts: moves and wheel deltas are truncated toward zero.
MouseMoveCommand move_command(double dx, double dy);
MouseButtonCommand button_command(MouseButton button, ButtonAction action);
WheelCommand wheel_command(double dx, double dy);

}  // namespace manus::inject
