#include "ws/session.hpp"

#include <cstddef>

#include <lorica/byte_view.hpp>
#include <lorica/input_budget.hpp>
#include <nlohmann/json.hpp>

#include "inject/input_plan.hpp"
#include "inject/keyboard.hpp"
#include "inject/mouse.hpp"
#include "util/js_number.hpp"
#include "ws/protocol.hpp"

namespace manus::ws {
namespace {

std::string quote(std::string_view s) {
    return nlohmann::json(std::string(s)).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string with_seq(std::string head, const std::optional<double>& seq) {
    if (seq) head += R"(,"seq":)" + util::js_number_to_string(*seq);
    return head + "}";
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace

// session.ts toCommand (ping is answered before this).
CommandResult to_command(const ClientMessage& message) {
    return std::visit(
        overloaded{
            [](const TextMessage& m) -> CommandResult { return {inject::text_command(m.text), {}}; },
            [](const KeyMessage& m) -> CommandResult {
                auto r = inject::key_command(m.key, m.modifiers, m.action.value_or(inject::KeyAction::press));
                if (!r.command) return {std::nullopt, r.error};
                return {std::move(*r.command), {}};
            },
            [](const MouseMoveMessage& m) -> CommandResult { return {inject::move_command(m.dx, m.dy), {}}; },
            [](const MouseButtonMessage& m) -> CommandResult { return {inject::button_command(m.button, m.action), {}}; },
            [](const MouseWheelMessage& m) -> CommandResult {
                return {inject::wheel_command(m.dx.value_or(0), m.dy.value_or(0)), {}};
            },
            [](const PingMessage&) -> CommandResult { return {std::nullopt, "ping is not a command"}; },
        },
        message);
}

std::string pong_json(const std::optional<double>& seq) { return with_seq(R"({"type":"pong")", seq); }

std::string ack_json(double seq) { return R"({"type":"ack","seq":)" + util::js_number_to_string(seq) + "}"; }

std::string error_json(std::string_view code, std::string_view message, const std::optional<double>& seq) {
    return with_seq(R"({"type":"error","code":)" + quote(code) + R"(,"message":)" + quote(message), seq);
}

std::optional<std::string> Session::on_message(std::string_view raw) {
    // One budget for this message, from admission to the parsed command. The
    // transport already refuses frames over 64 KiB (close 1009); this check keeps
    // the session safe on its own (fuzzing, other transports).
    lorica::input_budget budget(message_budget_limits());
    if (!lorica::byte_view::make(reinterpret_cast<const std::byte*>(raw.data()), raw.size(), kMessageLimits.max_bytes)) {
        return error_json("bad_message", "message is too large", std::nullopt);
    }
    const ParseResult parsed = parse_client_message(raw, budget);
    if (!parsed.message) return error_json("bad_message", parsed.error, std::nullopt);
    const ClientMessage& message = *parsed.message;
    const auto seq = seq_of(message);
    if (std::holds_alternative<PingMessage>(message)) return pong_json(seq);

    const CommandResult command = to_command(message);
    if (!command.command) return error_json("inject_failed", command.error, seq);
    // Node: helper.send() rejected with "helper_down" before the helper saw the command.
    if (!sink_.available()) return error_json("inject_failed", "helper_down", seq);
    const inject::InputPlan plan = inject::plan_input(*command.command);
    if (!plan.ok) return error_json("inject_failed", plan.error, seq);
    const inject::SinkResult sent = sink_.send(plan.events);
    if (!sent.ok) return error_json("inject_failed", sent.error, seq);
    if (seq) return ack_json(*seq);
    return std::nullopt;
}

}  // namespace manus::ws
