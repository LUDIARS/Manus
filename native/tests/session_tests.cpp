// src/ws/session.ts dispatch with SendInput replaced by FakeInputSink.
#include <string>

#include "fake_input_sink.hpp"
#include "inject/input_plan.hpp"
#include "test_harness.hpp"
#include "ws/session.hpp"

using manus::test::FakeInputSink;
using manus::ws::Session;

namespace {
std::string reply(Session& s, const std::string& raw) { return s.on_message(raw).value_or("<none>"); }
}  // namespace

TEST_CASE(ping_answers_pong_without_injecting) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"ping","seq":7})"), std::string(R"({"type":"pong","seq":7})"));
    CHECK_EQ(reply(s, R"({"type":"ping"})"), std::string(R"({"type":"pong"})"));
    CHECK(sink.calls.empty());
}

TEST_CASE(injection_acks_only_with_seq) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"key","key":"c","modifiers":["ctrl"],"seq":1})"), std::string(R"({"type":"ack","seq":1})"));
    CHECK_EQ(reply(s, R"({"type":"text","text":"x"})"), std::string("<none>"));
    REQUIRE(sink.calls.size() == 2);
    CHECK_EQ(sink.calls[0].size(), std::size_t{4});  // ctrl down, c down, c up, ctrl up
    CHECK_EQ(sink.calls[1].size(), std::size_t{2});
}

TEST_CASE(seq_is_echoed_with_js_number_formatting) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"ping","seq":1.0})"), std::string(R"({"type":"pong","seq":1})"));
    CHECK_EQ(reply(s, R"({"type":"ping","seq":-0})"), std::string(R"({"type":"pong","seq":0})"));
    CHECK_EQ(reply(s, R"({"type":"ping","seq":1.5e-7})"), std::string(R"({"type":"pong","seq":1.5e-7})"));
    CHECK_EQ(reply(s, R"({"type":"ping","seq":1e21})"), std::string(R"({"type":"pong","seq":1e+21})"));
    CHECK_EQ(reply(s, R"({"type":"ping","seq":12345678901234567890})"),
             std::string(R"({"type":"pong","seq":12345678901234567000})"));
}

TEST_CASE(bad_message_has_no_seq) {
    FakeInputSink sink;
    Session s(sink);
    const std::string r = reply(s, R"({"type":"key","key":"","seq":5})");
    CHECK(r.rfind(R"({"type":"error","code":"bad_message","message":)", 0) == 0);
    CHECK(r.find("\"seq\"") == std::string::npos);
    CHECK(sink.calls.empty());
}

TEST_CASE(unknown_key_is_inject_failed_with_seq) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"key","key":"banana","seq":3})"),
             std::string(R"({"type":"error","code":"inject_failed","message":"unknown key name: banana","seq":3})"));
    CHECK_EQ(reply(s, R"({"type":"key","key":"constructor"})"),
             std::string(R"({"type":"error","code":"inject_failed","message":"unknown key name: constructor"})"));
    CHECK(sink.calls.empty());
}

TEST_CASE(sink_down_is_helper_down) {
    FakeInputSink sink;
    sink.up = false;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"mouse.move","dx":1,"dy":1,"seq":2})"),
             std::string(R"({"type":"error","code":"inject_failed","message":"helper_down","seq":2})"));
    // the key name is still checked first, as toCommand ran before helper.send
    CHECK(reply(s, R"({"type":"key","key":"nope"})").find("unknown key name") != std::string::npos);
}

TEST_CASE(sink_failure_and_int32_overflow_are_inject_failed) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"mouse.move","dx":3000000000,"dy":0,"seq":4})"),
             std::string(R"({"type":"error","code":"inject_failed","message":"Value was either too large or too small for an Int32.","seq":4})"));
    sink.fail_with = "SendInput injected 0/2 (err=5)";
    CHECK_EQ(reply(s, R"({"type":"text","text":"a","seq":5})"),
             std::string(R"j({"type":"error","code":"inject_failed","message":"SendInput injected 0/2 (err=5)","seq":5})j"));
}

TEST_CASE(wheel_defaults_missing_axes_to_zero) {
    FakeInputSink sink;
    Session s(sink);
    CHECK_EQ(reply(s, R"({"type":"mouse.wheel","seq":1})"), std::string(R"({"type":"ack","seq":1})"));
    REQUIRE(sink.calls.size() == 1);
    CHECK(sink.calls[0].empty());  // nothing to inject, still acked (the helper sent nothing and said ok)
    reply(s, R"({"type":"mouse.wheel","dy":-240.9})");
    REQUIRE(sink.calls.size() == 2);
    REQUIRE(sink.calls[1].size() == 1);
    CHECK_EQ(sink.calls[1][0].mouse_data, static_cast<std::uint32_t>(-240));
    CHECK_EQ(sink.calls[1][0].flags, manus::inject::kMouseEventWheel);
}

TEST_CASE(error_message_is_json_escaped) {
    FakeInputSink sink;
    Session s(sink);
    const std::string r = reply(s, R"({"type":"key","key":"a\"\\\n"})");
    CHECK_EQ(r, std::string(R"({"type":"error","code":"inject_failed","message":"unknown key name: a\"\\\n"})"));
}
