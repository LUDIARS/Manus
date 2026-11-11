#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace manus::test {

// A fresh directory under the system temp dir, removed on destruction.
class TempDir {
public:
    explicit TempDir(const std::string& prefix = "manus-test-") {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                (prefix + std::to_string(stamp) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }

    void write(const std::filesystem::path& relative, const std::string& bytes) const {
        const auto full = path_ / relative;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

private:
    std::filesystem::path path_;
};

}  // namespace manus::test
