#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "inject/command.hpp"

namespace manus::inject {

// The OS-independent part of helper/ManusInputHelper.cs: which INPUT records
// one command becomes. The Win32 adapter copies them into INPUT structs and
// calls SendInput; tests compare them directly.

// Win32 flag values (winuser.h), duplicated so this file needs no Windows header.
inline constexpr std::uint32_t kKeyEventExtendedKey = 0x0001;
inline constexpr std::uint32_t kKeyEventKeyUp = 0x0002;
inline constexpr std::uint32_t kKeyEventUnicode = 0x0004;
inline constexpr std::uint32_t kMouseEventMove = 0x0001;
inline constexpr std::uint32_t kMouseEventLeftDown = 0x0002;
inline constexpr std::uint32_t kMouseEventLeftUp = 0x0004;
inline constexpr std::uint32_t kMouseEventRightDown = 0x0008;
inline constexpr std::uint32_t kMouseEventRightUp = 0x0010;
inline constexpr std::uint32_t kMouseEventMiddleDown = 0x0020;
inline constexpr std::uint32_t kMouseEventMiddleUp = 0x0040;
inline constexpr std::uint32_t kMouseEventWheel = 0x0800;
inline constexpr std::uint32_t kMouseEventHWheel = 0x1000;
inline constexpr std::uint16_t kVkReturn = 0x0D;
inline constexpr std::uint16_t kVkTab = 0x09;

struct InputEvent {
    enum class Type : std::uint8_t { mouse = 0, keyboard = 1 };  // INPUT_MOUSE / INPUT_KEYBOARD
    Type type = Type::keyboard;
    // keyboard (KEYBDINPUT)
    std::uint16_t vk = 0;
    std::uint16_t scan = 0;
    // mouse (MOUSEINPUT)
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    std::uint32_t mouse_data = 0;
    // dwFlags of either
    std::uint32_t flags = 0;

    bool operator==(const InputEvent&) const = default;
};

// More INPUT records than this from one message are refused (a 4096-character
// text is at most 8192 records).
inline constexpr std::size_t kMaxEventsPerCommand = 16384;

struct InputPlan {
    bool ok = false;
    std::vector<InputEvent> events;
    std::string error;  // when !ok (same texts the helper reported)
};

// VKs that need KEYEVENTF_EXTENDEDKEY: PgUp PgDn End Home arrows Ins Del LWin RWin NumDiv NumLock.
bool is_extended_vk(std::uint16_t vk);

InputPlan plan_input(const InjectCommand& command, std::size_t max_events = kMaxEventsPerCommand);

}  // namespace manus::inject
