#include "ws/protocol.hpp"

#include "util/utf.hpp"

namespace manus::ws {
namespace {

using json = nlohmann::json;

// z.number(): a JSON number (JSON cannot express NaN; overflow is a parse error).
std::optional<double> number_field(const json& obj, const char* name, bool required, std::string& error) {
    const auto it = obj.find(name);
    if (it == obj.end()) {
        if (required) error = std::string(name) + ": Required";
        return std::nullopt;
    }
    if (!it->is_number()) {
        error = std::string(name) + ": Expected number";
        return std::nullopt;
    }
    return it->get<double>();
}

const std::string* string_field(const json& obj, const char* name, std::string& error) {
    const auto it = obj.find(name);
    if (it == obj.end()) {
        error = std::string(name) + ": Required";
        return nullptr;
    }
    if (!it->is_string()) {
        error = std::string(name) + ": Expected string";
        return nullptr;
    }
    return &it->get_ref<const std::string&>();
}

// Optional "seq": absent is fine, present must be a number.
bool read_seq(const json& obj, std::optional<double>& seq, std::string& error) {
    if (!obj.contains("seq")) return true;
    seq = number_field(obj, "seq", false, error);
    return error.empty();
}

std::optional<inject::KeyAction> parse_key_action(std::string_view s) {
    if (s == "press") return inject::KeyAction::press;
    if (s == "down") return inject::KeyAction::down;
    if (s == "up") return inject::KeyAction::up;
    return std::nullopt;
}

std::optional<inject::MouseButton> parse_button(std::string_view s) {
    if (s == "left") return inject::MouseButton::left;
    if (s == "right") return inject::MouseButton::right;
    if (s == "middle") return inject::MouseButton::middle;
    return std::nullopt;
}

std::optional<inject::ButtonAction> parse_button_action(std::string_view s) {
    if (s == "down") return inject::ButtonAction::down;
    if (s == "up") return inject::ButtonAction::up;
    if (s == "click") return inject::ButtonAction::click;
    return std::nullopt;
}

ParseResult fail(std::string message) { return {std::nullopt, std::move(message)}; }

ParseResult parse_text(const json& obj) {
    TextMessage m;
    std::string error;
    const std::string* text = string_field(obj, "text", error);
    if (!text) return fail(error);
    if (util::utf16_length(*text) > kMaxTextUnits) return fail("text: String must contain at most 4096 character(s)");
    m.text = *text;
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

ParseResult parse_key(const json& obj) {
    KeyMessage m;
    std::string error;
    const std::string* key = string_field(obj, "key", error);
    if (!key) return fail(error);
    const std::size_t units = util::utf16_length(*key);
    if (units < 1) return fail("key: String must contain at least 1 character(s)");
    if (units > kMaxKeyUnits) return fail("key: String must contain at most 16 character(s)");
    m.key = *key;
    if (const auto it = obj.find("modifiers"); it != obj.end()) {
        if (!it->is_array()) return fail("modifiers: Expected array");
        for (const auto& item : *it) {
            const auto mod = item.is_string() ? inject::parse_modifier(item.get_ref<const std::string&>()) : std::nullopt;
            if (!mod) return fail("modifiers: Invalid enum value. Expected 'ctrl' | 'shift' | 'alt' | 'win'");
            m.modifiers.push_back(*mod);
        }
    }
    if (const auto it = obj.find("action"); it != obj.end()) {
        const auto action = it->is_string() ? parse_key_action(it->get_ref<const std::string&>()) : std::nullopt;
        if (!action) return fail("action: Invalid enum value. Expected 'press' | 'down' | 'up'");
        m.action = action;
    }
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

ParseResult parse_move(const json& obj) {
    MouseMoveMessage m;
    std::string error;
    const auto dx = number_field(obj, "dx", true, error);
    if (!dx) return fail(error);
    const auto dy = number_field(obj, "dy", true, error);
    if (!dy) return fail(error);
    m.dx = *dx;
    m.dy = *dy;
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

ParseResult parse_button_message(const json& obj) {
    MouseButtonMessage m;
    std::string error;
    const std::string* button = string_field(obj, "button", error);
    if (!button) return fail(error);
    const auto b = parse_button(*button);
    if (!b) return fail("button: Invalid enum value. Expected 'left' | 'right' | 'middle'");
    const std::string* action = string_field(obj, "action", error);
    if (!action) return fail(error);
    const auto a = parse_button_action(*action);
    if (!a) return fail("action: Invalid enum value. Expected 'down' | 'up' | 'click'");
    m.button = *b;
    m.action = *a;
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

ParseResult parse_wheel(const json& obj) {
    MouseWheelMessage m;
    std::string error;
    if (obj.contains("dx")) {
        m.dx = number_field(obj, "dx", false, error);
        if (!error.empty()) return fail(error);
    }
    if (obj.contains("dy")) {
        m.dy = number_field(obj, "dy", false, error);
        if (!error.empty()) return fail(error);
    }
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

ParseResult parse_ping(const json& obj) {
    PingMessage m;
    std::string error;
    if (!read_seq(obj, m.seq, error)) return fail(error);
    return {m, {}};
}

}  // namespace

ParseResult parse_client_message(std::string_view raw) {
    auto parsed = util::parse_bounded_json(raw, kMessageLimits);
    if (!parsed.value) return fail(parsed.error);
    const json& obj = *parsed.value;
    if (!obj.is_object()) return fail("Expected object");
    const auto type = obj.find("type");
    if (type == obj.end() || !type->is_string()) {
        return fail("Invalid discriminator value. Expected 'text' | 'key' | 'mouse.move' | 'mouse.button' | "
                    "'mouse.wheel' | 'ping'");
    }
    const std::string& t = type->get_ref<const std::string&>();
    if (t == "text") return parse_text(obj);
    if (t == "key") return parse_key(obj);
    if (t == "mouse.move") return parse_move(obj);
    if (t == "mouse.button") return parse_button_message(obj);
    if (t == "mouse.wheel") return parse_wheel(obj);
    if (t == "ping") return parse_ping(obj);
    return fail("Invalid discriminator value. Expected 'text' | 'key' | 'mouse.move' | 'mouse.button' | "
                "'mouse.wheel' | 'ping'");
}

std::optional<double> seq_of(const ClientMessage& message) {
    return std::visit([](const auto& m) { return m.seq; }, message);
}

}  // namespace manus::ws
