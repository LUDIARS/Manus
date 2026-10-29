#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace manus::platform {

enum class FileReadStatus { ok, missing, too_large, io_error };

struct FileReadResult {
    FileReadStatus status = FileReadStatus::io_error;
    std::string bytes;
};

// Reads a whole regular file of at most max_bytes. The size is charged to a
// lorica::input_budget before any buffer is allocated; a file that grows
// while being read is still cut off at max_bytes + 1 and reported as too large.
FileReadResult read_file_bounded(const std::filesystem::path& path, std::size_t max_bytes);

// Writes bytes to path atomically (temporary file in the same directory, then
// rename). On POSIX the file is created with mode 0600. Returns false on error.
bool write_file_atomic(const std::filesystem::path& path, const std::string& bytes, std::string& error);

}  // namespace manus::platform
