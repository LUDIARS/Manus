import type { HelperCommand } from "./helper-manager.js";

export function moveCommand(dx: number, dy: number): HelperCommand {
  return { op: "mouse_move", dx: Math.trunc(dx), dy: Math.trunc(dy) };
}

export function buttonCommand(
  button: "left" | "right" | "middle",
  action: "down" | "up" | "click",
): HelperCommand {
  return { op: "mouse_button", button, action };
}

export function wheelCommand(dx: number, dy: number): HelperCommand {
  return { op: "wheel", dx: Math.trunc(dx), dy: Math.trunc(dy) };
}
