// Regression runner for a libFuzzer entry without libFuzzer: calls
// LLVMFuzzerTestOneInput once for every file in the corpus directories given on
// the command line. Built with every compiler, so the GCC build (LUDIARS CI) and
// the sanitizer builds run the fuzz entries on the checked-in seeds.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>
#include <vector>

#include <lorica/input_budget.hpp>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

// Corpus files are small seeds; anything bigger is a mistake in the corpus.
constexpr std::size_t kMaxSeedBytes = 1024 * 1024;

bool run_file(const std::filesystem::path& file) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    lorica::input_budget budget({kMaxSeedBytes, 1, 1, 0});
    if (ec || !budget.consume(static_cast<std::size_t>(size), 1, 1)) {
        std::cerr << "seed unreadable or larger than 1 MiB: " << file << "\n";
        return false;
    }
    std::ifstream in(file, std::ios::binary);
    std::vector<char> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty() && !in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        std::cerr << "seed could not be read: " << file << "\n";
        return false;
    }
    LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t ran = 0;
    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(argv[i])) {
            if (entry.is_regular_file()) files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            ok = run_file(file) && ok;
            ++ran;
        }
    }
    // An empty input is always part of the regression run.
    LLVMFuzzerTestOneInput(nullptr, 0);
    std::cout << "ran " << ran << " seed(s) + the empty input\n";
    return ok && ran > 0 ? 0 : 1;
}
