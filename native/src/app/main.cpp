#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

#include "app/cli.hpp"
#include "app/manus_app.hpp"
#include "config/config_loader.hpp"
#include "log/jsonl_logger.hpp"
#include "platform/environment.hpp"
#include "transport/server.hpp"

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace manus;

std::filesystem::path resolve_root(const app::CliCommand& command) {
    std::error_code ec;
    const auto root = command.root.empty() ? std::filesystem::current_path(ec) : command.root;
    return std::filesystem::absolute(root, ec);
}

std::filesystem::path logs_dir_for(const std::filesystem::path& root) {
    const auto env = platform::get_env("VESTIGIUM_LOGS_DIR");
    if (env && env->find_first_not_of(" \t\r\n") != std::string::npos) return log::default_logs_dir();
    return root / "logs";
}

int run_service(const std::filesystem::path& root) {
    log::JsonlLogger logger({"manus", logs_dir_for(root), 14, true, true});
    const auto loaded = config::load_config(root);
    if (!loaded.config) {
        logger.error("[fatal] 設定を読めません: " + loaded.error);
        return 1;
    }
    const config::ManusConfig& cfg = *loaded.config;

    asio::io_context io(1);
    app::ManusApp application(false, logger);
    transport::Server server(io, application, logger);

    boost::system::error_code ec;
    tcp::resolver resolver(io);
    const auto endpoints = resolver.resolve(cfg.bind, std::to_string(cfg.port), tcp::resolver::passive, ec);
    if (ec || endpoints.empty()) {
        logger.error("[fatal] bind アドレスを解決できません: " + cfg.bind);
        return 1;
    }
    ec = server.listen(endpoints.begin()->endpoint());
    if (ec) {
        logger.error("[fatal] listen に失敗しました (" + cfg.bind + ":" + std::to_string(cfg.port) + "): " + ec.message());
        return 1;
    }
    logger.info("[manus] listening on http://" + cfg.bind + ":" + std::to_string(cfg.port) + " (helper down)");

    asio::signal_set signals(io, SIGINT, SIGTERM);
#ifdef SIGBREAK
    signals.add(SIGBREAK);
#endif
    signals.async_wait([&](boost::system::error_code sec, int) {
        if (sec) return;
        logger.info("[manus] shutting down");
        server.stop();
    });
    io.run();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    const app::CliCommand command = app::parse_cli(args);
    try {
        switch (command.kind) {
            case app::CommandKind::help:
                std::cout << app::usage_text();
                return 0;
            case app::CommandKind::error:
                std::cerr << command.error << "\n" << app::usage_text();
                return 1;
            case app::CommandKind::set_pin:
                std::cerr << "set-pin is not available yet\n";
                return 1;
            case app::CommandKind::serve:
                return run_service(resolve_root(command));
        }
    } catch (const std::exception& ex) {
        std::cerr << "[fatal] " << ex.what() << std::endl;
        return 1;
    }
    return 1;
}
