#include "inject/win32_input_sink.hpp"

#include <cstdint>
#include <vector>

#include "inject/batch_dispatch.hpp"

namespace manus::inject {
namespace {

class Win32InputSink final : public InputSink {
public:
    explicit Win32InputSink(SendInputFn send_input) : send_input_(send_input) {}

    bool available() const override { return send_input_ != nullptr; }

    // The rules around the call (empty plan, size limit, short count) live in
    // dispatch_in_one_call and are tested on every platform; this adapter only
    // converts the records and makes the one SendInput call.
    SinkResult send(const std::vector<InputEvent>& events) override {
        return dispatch_in_one_call(events, sizeof(INPUT), [this](const std::vector<InputEvent>& planned) {
            std::vector<INPUT> inputs;
            inputs.reserve(planned.size());
            for (const auto& e : planned) inputs.push_back(to_win32_input(e));
            // dispatch_in_one_call has checked count * sizeof(INPUT) <= INT_MAX.
            const UINT sent =
                send_input_(static_cast<UINT>(inputs.size()), inputs.data(), static_cast<int>(sizeof(INPUT)));
            BatchOutcome outcome;
            outcome.injected = static_cast<std::uint32_t>(sent);
            if (sent != inputs.size()) outcome.last_error = static_cast<std::uint32_t>(::GetLastError());
            return outcome;
        });
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
