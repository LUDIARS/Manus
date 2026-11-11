// Lorica is applied to this translation unit: checked from inside it.
//
// Built twice through manus_target() (tests/CMakeLists.txt): ctest
// "manus.lorica_applied.baseline" (manus_target(<t>)) and
// "manus.lorica_applied.extended" (manus_target(<t> SERVER)). The build passes what
// Lorica's own rules (cmake/LoricaHardening.cmake) make it expect:
//   MANUS_LORICA_EXPECT_EXTENDED  1 when the target takes the EXTENDED profile
//   MANUS_LORICA_EXPECT_FORTIFY   3 in optimized Linux configurations, else 0
// Each check is recorded as a boolean here and asserted at run time, so a missing
// setting fails ctest with its name rather than stopping the build.
//
// What this unit cannot see is checked by "manus.lorica_applied.compile_commands"
// (tests/tools/check_lorica_compile_commands.cmake): the stack protector, stack clash
// and control-flow protection, -Werror=deprecated-declarations, and EXTENDED's
// -ftrivial-auto-var-init=zero with libstdc++ (none of them defines a macro).

// ---- 1. lorica/unsafe_api.hpp is force-included (must stay before every #include).
// The header uses #pragma once and #undefs its helper macros, so it leaves no macro
// to test with #ifdef. Its lasting mark is a declaration: it declares ::strcpy (as
// deprecated) ahead of the unit's first line. Before any #include here, ::strcpy is
// declared only if the header was force-included. The lookup below finds either
// ::strcpy (the non-template wins) or the fallback template, which returns
// probe::absent; the deprecation is silenced only for this unevaluated probe.
namespace manus_lorica_probe {
struct absent {};
template <class... Args>
absent strcpy(Args...);
template <class T>
T make();  // only named in decltype (like std::declval, which needs a header)
template <class T, class U>
struct same {
    static constexpr bool value = false;
};
template <class T>
struct same<T, T> {
    static constexpr bool value = true;
};
}  // namespace manus_lorica_probe

namespace manus_lorica_probe_lookup {
using namespace manus_lorica_probe;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
inline constexpr bool unsafe_api_declared_before_first_line =
    !manus_lorica_probe::same<decltype(strcpy(make<char*>(), make<const char*>())),
                              manus_lorica_probe::absent>::value;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}  // namespace manus_lorica_probe_lookup

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

#include <lorica/checked_integer.hpp>
#include <lorica/input_budget.hpp>

#include "test_harness.hpp"

#if !defined(MANUS_LORICA_EXPECT_EXTENDED) || !defined(MANUS_LORICA_EXPECT_FORTIFY)
#error "tests/CMakeLists.txt passes MANUS_LORICA_EXPECT_EXTENDED and MANUS_LORICA_EXPECT_FORTIFY"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define MANUS_LORICA_MSVC 1
#else
#define MANUS_LORICA_MSVC 0
#endif

TEST_CASE(unsafe_api_header_is_force_included) {
#if MANUS_LORICA_MSVC
    // MSVC: lorica_deprecate_unsafe_apis adds /we4996 and no header (the UCRT itself
    // deprecates strcpy and friends). /we4996 defines no macro, so it is not visible
    // here; the visible way to lose it is _CRT_SECURE_NO_WARNINGS, which turns the
    // UCRT deprecations off. Not run on this branch in the cloud (no MSVC there).
    CHECK(!manus_lorica_probe_lookup::unsafe_api_declared_before_first_line);
#if defined(_CRT_SECURE_NO_WARNINGS)
    CHECK(!"_CRT_SECURE_NO_WARNINGS turns off the unsafe API rejection");
#endif
#else
    CHECK(manus_lorica_probe_lookup::unsafe_api_declared_before_first_line);
#endif
}

TEST_CASE(standard_library_hardening_matches_the_profile) {
    constexpr bool extended = MANUS_LORICA_EXPECT_EXTENDED != 0;
#if MANUS_LORICA_MSVC
    // MSVC: the baseline (/GS /sdl /guard:cf) defines no macro; EXTENDED defines the
    // STL hardening switches for C++. Not run in the cloud (no MSVC there).
#if defined(_MSVC_STL_HARDENING) && _MSVC_STL_HARDENING == 1 && defined(_MSVC_STL_DESTRUCTOR_TOMBSTONES) && \
    _MSVC_STL_DESTRUCTOR_TOMBSTONES == 1
    constexpr bool stl_hardened = true;
#else
    constexpr bool stl_hardened = false;
#endif
    CHECK_EQ(stl_hardened, extended);
#else
    // GCC / Clang on Linux: Lorica defines _GLIBCXX_ASSERTIONS and
    // _LIBCPP_HARDENING_MODE for every C++ unit (whichever library is in use):
    // _LIBCPP_HARDENING_MODE_FAST for the baseline, _EXTENSIVE for EXTENDED.
#if defined(_GLIBCXX_ASSERTIONS)
    constexpr bool glibcxx_assertions = true;
#else
    constexpr bool glibcxx_assertions = false;
#endif
#if defined(_LIBCPP_HARDENING_MODE)
    constexpr bool libcpp_mode_defined = true;
#else
    constexpr bool libcpp_mode_defined = false;
#endif
    CHECK(glibcxx_assertions);
    CHECK(libcpp_mode_defined);
#if defined(_LIBCPP_VERSION)
    // libc++ gives the mode names values; libstdc++ leaves them unset (the value is
    // only meaningful to libc++, so it is compared only there).
#if _LIBCPP_HARDENING_MODE == _LIBCPP_HARDENING_MODE_EXTENSIVE
    constexpr bool libcpp_extensive = true;
#else
    constexpr bool libcpp_extensive = false;
#endif
#if _LIBCPP_HARDENING_MODE == _LIBCPP_HARDENING_MODE_FAST
    constexpr bool libcpp_fast = true;
#else
    constexpr bool libcpp_fast = false;
#endif
    CHECK_EQ(libcpp_extensive, extended);
    CHECK_EQ(libcpp_fast, !extended);
#elif defined(__GLIBCXX__)
    // libstdc++: the assertions are the whole hardened mode; EXTENDED adds only
    // compiler flags, which the compile_commands test checks.
    (void)extended;
#else
    CHECK(!"unknown standard library: Lorica supports libstdc++ and libc++ here");
#endif
#endif
}

TEST_CASE(fortify_level_matches_the_configuration) {
#if MANUS_LORICA_EXPECT_FORTIFY
    // Lorica's -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 (optimized Linux configurations).
#if defined(_FORTIFY_SOURCE)
    CHECK_EQ(static_cast<int>(_FORTIFY_SOURCE), MANUS_LORICA_EXPECT_FORTIFY);
#else
    CHECK(!"_FORTIFY_SOURCE is not defined in an optimized configuration");
#endif
#if defined(__GLIBC__)
    // glibc's own view of the level (<cstring> above read features.h).
#if defined(__USE_FORTIFY_LEVEL)
    CHECK_EQ(static_cast<int>(__USE_FORTIFY_LEVEL), MANUS_LORICA_EXPECT_FORTIFY);
#else
    CHECK(!"glibc did not enable FORTIFY");
#endif
#endif
#else
    // Unoptimized configurations and Windows: Lorica adds no FORTIFY level (MSVC has
    // none), so there is nothing to compare.
    CHECK(true);
#endif
}

TEST_CASE(lorica_boundary_checked_add_rejects_overflow) {
    // Values from a vector so the calls run at run time, not in a constant.
    const std::vector<std::uint32_t> operands{std::numeric_limits<std::uint32_t>::max(), 1U, 2U};
    const std::optional<std::uint32_t> overflow = lorica::checked_add(operands[0], operands[1]);
    CHECK(!overflow.has_value());
    const std::optional<std::uint32_t> sum = lorica::checked_add(operands[1], operands[2]);
    REQUIRE(sum.has_value());
    CHECK_EQ(*sum, 3U);
    const std::vector<std::int64_t> signed_operands{std::numeric_limits<std::int64_t>::min(), -1};
    CHECK(!lorica::checked_add(signed_operands[0], signed_operands[1]).has_value());
}

TEST_CASE(lorica_boundary_input_budget_limit_exact_and_plus_one) {
    const std::vector<std::size_t> sizes{4096, 4097};
    const lorica::budget_limits limits{sizes[0], 8, 8, 4};
    lorica::input_budget exact(limits);
    CHECK(exact.consume(sizes[0], 1, 1));
    CHECK(!exact.consume(1, 0, 0));  // the limit is spent
    lorica::input_budget over(limits);
    CHECK(!over.consume(sizes[1], 1, 1));
    CHECK(over.consume(sizes[0], 1, 1));  // a refused charge takes nothing
    CHECK(over.permits_depth(4));
    CHECK(!over.permits_depth(5));
}
