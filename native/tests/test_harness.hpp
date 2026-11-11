#pragma once

// Minimal test harness (no third-party test framework). Each test file defines
// TEST_CASE functions; test_main.cpp runs them all and returns non-zero on failure.

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace manus::test {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();
void record_failure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

template <class A, class B>
std::string describe_mismatch(const A& a, const B& b) {
    std::ostringstream out;
    out << "  left:  " << a << "\n  right: " << b;
    return out.str();
}

}  // namespace manus::test

#define MANUS_TEST_CONCAT2(a, b) a##b
#define MANUS_TEST_CONCAT(a, b) MANUS_TEST_CONCAT2(a, b)

#define TEST_CASE(name)                                                                       \
    static void name();                                                                       \
    static const ::manus::test::Registrar MANUS_TEST_CONCAT(registrar_, name)(#name, &name); \
    static void name()

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) ::manus::test::record_failure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (false)

#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto manus_left = (a);                                                                       \
        const auto manus_right = (b);                                                                      \
        if (!(manus_left == manus_right)) {                                                                \
            ::manus::test::record_failure(__FILE__, __LINE__,                                              \
                                          "CHECK_EQ(" #a ", " #b ")\n" +                                   \
                                              ::manus::test::describe_mismatch(manus_left, manus_right)); \
        }                                                                                                  \
    } while (false)

// Stops the current test case (after recording the failure) when cond is false.
#define REQUIRE(cond)                                                                        \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            ::manus::test::record_failure(__FILE__, __LINE__, "REQUIRE(" #cond ")");         \
            return;                                                                          \
        }                                                                                    \
    } while (false)
