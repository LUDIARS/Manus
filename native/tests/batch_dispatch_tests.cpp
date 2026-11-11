// The rules around the one SendInput call (inject/batch_dispatch), with the call
// itself replaced by a fake. Runs on every platform; the Win32 conversion to INPUT
// and the real SendInput are covered by manus.win32_input_sink on Windows only.
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

#include "inject/batch_dispatch.hpp"
#include "inject/input_plan.hpp"
#include "inject/keyboard.hpp"
#include "test_harness.hpp"

using namespace manus::inject;

namespace {

// Stands in for SendInput: records the call and reports `accept` records injected.
struct FakeSendInput {
    std::uint32_t accept = UINT32_MAX;  // UINT32_MAX: all of them
    std::uint32_t error = 0;
    int calls = 0;
    std::vector<InputEvent> last;

    BatchSender sender() {
        return [this](const std::vector<InputEvent>& events) {
            ++calls;
            last = events;
            BatchOutcome out;
            out.injected = accept == UINT32_MAX ? static_cast<std::uint32_t>(events.size()) : accept;
            out.last_error = accept == UINT32_MAX ? 0 : error;
            return out;
        };
    }
};

constexpr std::size_t kWin32InputSize = 40;  // sizeof(INPUT) on x64

}  // namespace

TEST_CASE(empty_plan_succeeds_without_a_call) {
    FakeSendInput fake;
    const auto r = dispatch_in_one_call({}, kWin32InputSize, fake.sender());
    CHECK(r.ok);
    CHECK_EQ(fake.calls, 0);
}

TEST_CASE(all_records_go_in_one_call) {
    FakeSendInput fake;
    const auto plan = plan_input(text_command("ab"));
    REQUIRE(plan.ok);
    const auto r = dispatch_in_one_call(plan.events, kWin32InputSize, fake.sender());
    CHECK(r.ok);
    CHECK_EQ(fake.calls, 1);
    CHECK_EQ(fake.last.size(), std::size_t{4});
}

TEST_CASE(short_count_is_reported_like_the_helper) {
    FakeSendInput fake;
    fake.accept = 1;
    fake.error = 5;
    const auto r = dispatch_in_one_call(plan_input(text_command("ab")).events, kWin32InputSize, fake.sender());
    CHECK(!r.ok);
    CHECK_EQ(r.error, std::string("SendInput injected 1/4 (err=5)"));
}

TEST_CASE(array_size_limit_exact_and_plus_one) {
    // count * record_size must fit SendInput's int. Two records with a large record
    // size state the boundary without a huge vector: INT_MAX - 1 bytes is allowed,
    // INT_MAX + 1 is refused before the call.
    FakeSendInput fake;
    const std::vector<InputEvent> two(2);
    const std::size_t half = static_cast<std::size_t>(INT_MAX) / 2;  // 2 * half = INT_MAX - 1
    CHECK(dispatch_in_one_call(two, half, fake.sender()).ok);
    CHECK_EQ(fake.calls, 1);
    const auto r = dispatch_in_one_call(two, half + 1, fake.sender());  // 2 * (half + 1) = INT_MAX + 1
    CHECK(!r.ok);
    CHECK_EQ(r.error, std::string("too many input events for one message"));
    CHECK_EQ(fake.calls, 1);
    // count * record_size overflowing size_t is refused, not wrapped.
    CHECK(!dispatch_in_one_call(two, SIZE_MAX, fake.sender()).ok);
    CHECK_EQ(fake.calls, 1);
}
