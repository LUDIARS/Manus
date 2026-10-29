#include "inject/win32_input_sink.hpp"

#include <climits>
#include <string>
#include <vector>

namespace manus::inject {
namespace {

class Win32InputSink final : public InputSink {
public:
    explicit Win32InputSink(SendInputFn send_input) : send_input_(send_input) {}

    bool available() const override { return send_input_ != nullptr; }

    SinkResult send(const std::vector<InputEvent>& events) override {
        if (events.empty()) return {true, {}};
        if (events.size() > static_cast<std::size_t>(INT_MAX / sizeof(INPUT))) {
            return {false, "too many input events for one message"};
        }
        std::vector<INPUT> inputs;
        inputs.reserve(events.size());
        for (const auto& e : events) inputs.push_back(to_win32_input(e));
        const UINT total = static_cast<UINT>(inputs.size());
        const UINT sent = send_input_(total, inputs.data(), static_cast<int>(sizeof(INPUT)));
        if (sent != total) {
            return {false, "SendInput injected " + std::to_string(sent) + "/" + std::to_string(total) +
                               " (err=" + std::to_string(::GetLastError()) + ")"};
        }
        return {true, {}};
    }

private:
    SendInputFn send_input_;
};

}  // namespace

INPUT to_win32_input(const InputEvent& e) {
    INPUT input{};
    if (e.type == InputEvent::Type::keyboard) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = e.vk;
        input.ki.wScan = e.scan;
        input.ki.dwFlags = e.flags;
        input.ki.time = 0;
        input.ki.dwExtraInfo = 0;
    } else {
        input.type = INPUT_MOUSE;
        input.mi.dx = e.dx;
        input.mi.dy = e.dy;
        input.mi.mouseData = e.mouse_data;
        input.mi.dwFlags = e.flags;
        input.mi.time = 0;
        input.mi.dwExtraInfo = 0;
    }
    return input;
}

std::unique_ptr<InputSink> make_win32_input_sink(SendInputFn send_input) {
    return std::make_unique<Win32InputSink>(send_input);
}

std::unique_ptr<InputSink> make_platform_input_sink() { return make_win32_input_sink(&::SendInput); }

}  // namespace manus::inject
