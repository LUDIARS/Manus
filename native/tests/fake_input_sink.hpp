#pragma once

#include <string>
#include <vector>

#include "inject/input_sink.hpp"

namespace manus::test {

// Stands in for SendInput on every platform: records each call's records.
class FakeInputSink final : public inject::InputSink {
public:
    bool available() const override { return up; }
    inject::SinkResult send(const std::vector<inject::InputEvent>& events) override {
        if (!up) return {false, "helper_down"};
        if (!fail_with.empty()) return {false, fail_with};
        calls.push_back(events);
        return {true, {}};
    }

    bool up = true;
    std::string fail_with;
    std::vector<std::vector<inject::InputEvent>> calls;
};

}  // namespace manus::test
