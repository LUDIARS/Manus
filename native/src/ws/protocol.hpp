#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "inject/command.hpp"
#include "inject/jis_vk_map.hpp"
#include "util/bounded_json.hpp"

namespace manus::ws {

// Client -> server messages of src/ws/protocol.ts (zod discriminated union on
// "type"). Unknown extra fields are ignored, as zod's default "strip" did.

struct TextMessage {
    std::string text;  // at most 4096 UTF-16 code units
    std::optional<double> seq;
};

struct KeyMessage {
    std::string key;  // 1..16 UTF-16 code units
    std::vector<inject::Modifier> modifiers;
    std::optional<inject::KeyAction> action;
    std::optional<double> seq;
};

struct MouseMoveMessage {
    double dx = 0;
    double dy = 0;
    std::optional<double> seq;
};

struct MouseButtonMessage {
    inject::MouseButton button = inject::MouseButton::left;
    inject::ButtonAction action = inject::ButtonAction::click;
    std::optional<double> seq;
};

struct MouseWheelMessage {
    std::optional<double> dx;
    std::optional<double> dy;
    std::optional<double> seq;
};

struct PingMessage {
    std::optional<double> seq;
};

using ClientMessage =
    std::variant<TextMessage, KeyMessage, MouseMoveMessage, MouseButtonMessage, MouseWheelMessage, PingMessage>;

inline constexpr std::size_t kMaxTextUnits = 4096;
inline constexpr std::size_t kMaxKeyUnits = 16;

// One message is at most 64 KiB (the transport refuses bigger frames), 1024
// JSON values and 8 levels deep; all charged before and while parsing.
inline constexpr util::JsonLimits kMessageLimits{64 * 1024, 1024, 8};

struct ParseResult {
    std::optional<ClientMessage> message;
    std::string error;  // when message is empty
};

// Parses one message with its own budget (json_budget_limits(kMessageLimits)).
ParseResult parse_client_message(std::string_view raw);

// Same, charging the budget of the message being handled (Session::on_message).
ParseResult parse_client_message(std::string_view raw, lorica::input_budget& budget);

// The budget for one WebSocket message: everything parse_client_message charges.
lorica::budget_limits message_budget_limits();

std::optional<double> seq_of(const ClientMessage& message);

}  // namespace manus::ws
