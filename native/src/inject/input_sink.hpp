#pragma once

#include <memory>
#include <string>
#include <vector>

#include "inject/input_plan.hpp"

namespace manus::inject {

struct SinkResult {
    bool ok = false;
    std::string error;
};

// Where planned INPUT records go: SendInput on Windows, a recording fake in tests.
class InputSink {
public:
    virtual ~InputSink() = default;
    // Reported as "helper": "up" / "down" in /health.
    virtual bool available() const = 0;
    // Sends all records in one call. An empty list succeeds without a call.
    virtual SinkResult send(const std::vector<InputEvent>& events) = 0;
};

// Win32 SendInput on Windows; elsewhere a sink that is never available
// ("helper_down"), so the Linux build can run the HTTP/WS surface only.
std::unique_ptr<InputSink> make_platform_input_sink();

}  // namespace manus::inject
