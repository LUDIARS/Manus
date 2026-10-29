#include "test_harness.hpp"

#include <exception>

namespace manus::test {
namespace {
int g_failures = 0;
const char* g_current = "";
}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

void record_failure(const char* file, int line, const std::string& message) {
    ++g_failures;
    std::cerr << "FAIL [" << g_current << "] " << file << ":" << line << "\n" << message << "\n";
}

}  // namespace manus::test

int main() {
    using namespace manus::test;
    int failed_cases = 0;
    for (const auto& tc : registry()) {
        g_current = tc.name;
        const int before = g_failures;
        try {
            tc.body();
        } catch (const std::exception& ex) {
            record_failure(__FILE__, __LINE__, std::string("unexpected exception: ") + ex.what());
        } catch (...) {
            record_failure(__FILE__, __LINE__, "unexpected non-standard exception");
        }
        if (g_failures != before) {
            ++failed_cases;
            std::cout << "[FAIL] " << tc.name << "\n";
        } else {
            std::cout << "[ ok ] " << tc.name << "\n";
        }
    }
    std::cout << registry().size() - static_cast<std::size_t>(failed_cases) << "/" << registry().size()
              << " test cases passed\n";
    return failed_cases == 0 ? 0 : 1;
}
