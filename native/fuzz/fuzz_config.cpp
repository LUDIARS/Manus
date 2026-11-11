// libFuzzer entry: the text of manus.config.json (bounded JSON + strict schema)
// with the same limits as the loader.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "config/config.hpp"
#include "util/bounded_json.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto parsed = manus::util::parse_bounded_json(std::string_view(reinterpret_cast<const char*>(data), size),
                                                        manus::config::kConfigFileLimits);
    if (parsed.value) (void)manus::config::parse_config(*parsed.value);
    return 0;
}
