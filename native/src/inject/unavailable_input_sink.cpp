#include "inject/input_sink.hpp"

namespace manus::inject {
namespace {

// Non-Windows builds have no way to inject input. This is reported, never
// silently pretended: /health says "helper": "down" and every injection fails
// with the same error the Node server used when its helper was down.
class UnavailableInputSink final : public InputSink {
public:
    bool available() const override { return false; }
    SinkResult send(const std::vector<InputEvent>&) override { return {false, "helper_down"}; }
};

}  // namespace

std::unique_ptr<InputSink> make_platform_input_sink() { return std::make_unique<UnavailableInputSink>(); }

}  // namespace manus::inject
