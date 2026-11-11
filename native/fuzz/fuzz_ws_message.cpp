// libFuzzer entry: one WebSocket text message through the session (parse,
// validation, command, input plan) with an input sink that records nothing.
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "inject/input_sink.hpp"
#include "ws/session.hpp"

namespace {

class DiscardingSink final : public manus::inject::InputSink {
public:
    bool available() const override { return true; }
    manus::inject::SinkResult send(const std::vector<manus::inject::InputEvent>&) override { return {true, {}}; }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    DiscardingSink sink;
    manus::ws::Session session(sink);
    (void)session.on_message(std::string_view(reinterpret_cast<const char*>(data), size));
    return 0;
}
