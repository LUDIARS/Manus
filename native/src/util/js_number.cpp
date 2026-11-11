#include "util/js_number.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace manus::util {

std::string js_number_to_string(double value) {
    if (!std::isfinite(value)) return "null";
    if (value == 0.0) return "0";  // also -0

    // Shortest round-trip digits in scientific form: "-d.ddde+XX".
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                      std::chars_format::scientific);
    const std::string_view text(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));

    const bool negative = text.front() == '-';
    const std::size_t e_pos = text.find('e');
    std::string digits;
    for (std::size_t i = negative ? 1 : 0; i < e_pos; ++i) {
        if (text[i] != '.') digits.push_back(text[i]);
    }
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    const int exponent = std::atoi(std::string(text.substr(e_pos + 1)).c_str());

    const int k = static_cast<int>(digits.size());
    const int n = exponent + 1;  // position of the decimal point
    std::string out = negative ? "-" : "";
    if (k <= n && n <= 21) {
        out += digits;
        out.append(static_cast<std::size_t>(n - k), '0');
    } else if (0 < n && n <= 21) {
        out += digits.substr(0, static_cast<std::size_t>(n));
        out += '.';
        out += digits.substr(static_cast<std::size_t>(n));
    } else if (-6 < n && n <= 0) {
        out += "0.";
        out.append(static_cast<std::size_t>(-n), '0');
        out += digits;
    } else {
        out += digits.substr(0, 1);
        if (k > 1) {
            out += '.';
            out += digits.substr(1);
        }
        out += 'e';
        const int e = n - 1;
        out += e < 0 ? '-' : '+';
        out += std::to_string(e < 0 ? -e : e);
    }
    return out;
}

}  // namespace manus::util
