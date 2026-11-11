#pragma once

#include <string>

namespace manus::util {

// Formats a finite double the way ECMAScript Number::toString (and therefore
// JSON.stringify) does: shortest round-trip digits, plain notation for
// exponents in [-7, 21), "1e+21" / "1e-7" style otherwise, and "0" for -0.
// Non-finite values return "null" (what JSON.stringify writes for them).
std::string js_number_to_string(double value);

}  // namespace manus::util
