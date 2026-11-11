#include "inject/batch_dispatch.hpp"

#include <climits>
#include <string>

#include <lorica/checked_size.hpp>

namespace manus::inject {

SinkResult dispatch_in_one_call(const std::vector<InputEvent>& events, std::size_t record_size,
                                const BatchSender& send) {
    if (events.empty()) return {true, {}};
    // SendInput(UINT count, LPINPUT, int size): the whole array must be addressable
    // with an int, so count * record_size is computed with an overflow check.
    const auto bytes = lorica::checked_multiply(events.size(), record_size);
    if (!bytes || *bytes > static_cast<std::size_t>(INT_MAX)) {
        return {false, "too many input events for one message"};
    }
    const BatchOutcome outcome = send(events);
    if (outcome.injected != events.size()) {
        return {false, "SendInput injected " + std::to_string(outcome.injected) + "/" +
                           std::to_string(events.size()) + " (err=" + std::to_string(outcome.last_error) + ")"};
    }
    return {true, {}};
}

}  // namespace manus::inject
