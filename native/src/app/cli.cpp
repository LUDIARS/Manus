#include "app/cli.hpp"

namespace manus::app {

CliCommand parse_cli(const std::vector<std::string>& args) {
    CliCommand command;
    std::vector<std::string> positional;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--root") {
            if (i + 1 >= args.size()) {
                command.kind = CommandKind::error;
                command.error = "--root needs a directory";
                return command;
            }
            command.root = args[++i];
        } else if (arg == "--help" || arg == "-h") {
            command.kind = CommandKind::help;
            return command;
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            command.kind = CommandKind::error;
            command.error = "unknown option: " + arg;
            return command;
        } else {
            positional.push_back(arg);
        }
    }
    if (positional.empty() || positional[0] == "serve") {
        if (positional.size() > 1) {
            command.kind = CommandKind::error;
            command.error = "serve takes no arguments";
            return command;
        }
        command.kind = CommandKind::serve;
        return command;
    }
    if (positional[0] == "set-pin") {
        if (positional.size() != 2) {
            command.kind = CommandKind::error;
            command.error = "使い方: manus set-pin <4〜10桁の数字>";
            return command;
        }
        command.kind = CommandKind::set_pin;
        command.pin = positional[1];
        return command;
    }
    command.kind = CommandKind::error;
    command.error = "unknown command: " + positional[0];
    return command;
}

std::string usage_text() {
    return "使い方:\n"
           "  manus [serve] [--root <dir>]          サービスを起動する (既定)\n"
           "  manus set-pin <pin> [--root <dir>]    ペアリング PIN (4〜10 桁の数字) を設定する\n"
           "--root を省くと現在のディレクトリ (リポのルート) を使う。\n";
}

}  // namespace manus::app
