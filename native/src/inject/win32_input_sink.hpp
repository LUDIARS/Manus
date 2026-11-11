#pragma once

// Windows only.
#include <memory>

#include <windows.h>

#include "inject/input_sink.hpp"

namespace manus::inject {

// Signature of user32!SendInput. Tests pass a fake with the same signature.
using SendInputFn = UINT(WINAPI*)(UINT, LPINPUT, int);

// Copies planned records into INPUT structs and calls send_input once per command.
// A short count is an error: "SendInput injected <n>/<total> (err=<GetLastError>)".
std::unique_ptr<InputSink> make_win32_input_sink(SendInputFn send_input);

// Converts one planned record into the INPUT struct passed to SendInput.
INPUT to_win32_input(const InputEvent& event);

}  // namespace manus::inject
