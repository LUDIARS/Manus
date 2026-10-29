#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "inject/input_sink.hpp"

namespace manus::ws {

// One authorised connection's message dispatch (src/ws/session.ts):
//   invalid message         -> {"type":"error","code":"bad_message","message":...}
//   ping                    -> {"type":"pong","seq":n}            (seq omitted when absent)
//   injected                -> {"type":"ack","seq":n}             (only when seq was sent)
//   not injected            -> {"type":"error","code":"inject_failed","message":...,"seq":n}
// The ack is sent after the input was handed to the OS (the client's latency HUD measures this).
class Session {
public:
    explicit Session(inject::InputSink& sink) : sink_(sink) {}
    std::optional<std::string> on_message(std::string_view raw);

private:
    inject::InputSink& sink_;
};

// Reply builders (JSON.stringify key order and number formatting).
std::string pong_json(const std::optional<double>& seq);
std::string ack_json(double seq);
std::string error_json(std::string_view code, std::string_view message, const std::optional<double>& seq);

}  // namespace manus::ws
