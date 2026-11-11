#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace manus::app {

enum class CommandKind { serve, set_pin, help, error };

// manus [serve] [--root <dir>]
// manus set-pin <pin> [--root <dir>]
struct CliCommand {
    CommandKind kind = CommandKind::serve;
    std::filesystem::path root;  // empty: current directory (Excubitor starts us in the repository root)
    std::string pin;
    std::string error;
};

CliCommand parse_cli(const std::vector<std::string>& args);

std::string usage_text();

}  // namespace manus::app
