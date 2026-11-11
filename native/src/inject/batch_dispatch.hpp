#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "inject/input_plan.hpp"
#include "inject/input_sink.hpp"

namespace manus::inject {

// What one call of the OS input API reports: how many records it injected and,
// when that is short, the OS error code it left behind.
struct BatchOutcome {
    std::uint32_t injected = 0;
    std::uint32_t last_error = 0;
};

// Hands every planned record to the OS input API in a single call (SendInput
// takes a count, an array and the record size). Only the call is OS-specific;
// this function holds the rules around it, so they are tested on every platform:
//   - an empty plan succeeds without calling send;
//   - count * record_size must fit the API's int (checked, never wrapped);
//   - a short count is an error "SendInput injected <n>/<total> (err=<code>)",
//     the text the C# helper reported.
using BatchSender = std::function<BatchOutcome(const std::vector<InputEvent>&)>;

SinkResult dispatch_in_one_call(const std::vector<InputEvent>& events, std::size_t record_size,
                                const BatchSender& send);

}  // namespace manus::inject
