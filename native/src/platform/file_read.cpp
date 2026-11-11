#include "platform/file_read.hpp"

#include <fstream>
#include <system_error>

#include <lorica/input_budget.hpp>

namespace manus::platform {

FileReadResult read_file_bounded(const std::filesystem::path& path, std::size_t max_bytes) {
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(status)) return {FileReadStatus::missing, {}};
    if (!std::filesystem::is_regular_file(status)) return {FileReadStatus::io_error, {}};
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) return {FileReadStatus::io_error, {}};

    lorica::input_budget budget({max_bytes, 1, 1, 0});
    if (size > max_bytes || !budget.consume(static_cast<std::size_t>(size), 1, 1)) {
        return {FileReadStatus::too_large, {}};
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) return {FileReadStatus::io_error, {}};
    std::string bytes;
    bytes.resize(static_cast<std::size_t>(size));
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto got = static_cast<std::size_t>(in.gcount());
    bytes.resize(got);
    // A file that grew after file_size() is refused instead of being read past the charge.
    if (in.peek() != std::ifstream::traits_type::eof()) return {FileReadStatus::too_large, {}};
    return {FileReadStatus::ok, std::move(bytes)};
}

bool write_file_atomic(const std::filesystem::path& path, const std::string& bytes, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot create directory " + path.parent_path().string() + ": " + ec.message();
        return false;
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot open " + temp.string();
            return false;
        }
#ifndef _WIN32
        std::filesystem::permissions(temp, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace, ec);
        if (ec) {
            error = "cannot restrict permissions of " + temp.string() + ": " + ec.message();
            return false;
        }
#endif
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            error = "cannot write " + temp.string();
            return false;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        error = "cannot replace " + path.string();
        return false;
    }
    return true;
}

}  // namespace manus::platform
