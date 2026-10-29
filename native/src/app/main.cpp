#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

#include "app/cli.hpp"
#include "app/manus_app.hpp"
#include "auth/pairing_store.hpp"
#include "config/config_loader.hpp"
#include "inject/input_sink.hpp"
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

int run_set_pin(const std::filesystem::path& root, const std::string& pin) {
    std::string error;
    if (!auth::store_pin(root, pin, error)) {
        std::cerr << error << std::endl;
        return 1;
    }
    std::cout << "ペアリング PIN を設定しました。" << std::endl;
    return 0;
}

int run_service(const std::filesystem::path& root) {
    log::JsonlLogger logger({"manus", logs_dir_for(root), 14, true, true});
    const auto loaded = config::load_config(root);
    if (!loaded.config) {
        logger.error("[fatal] 設定を読めません: " + loaded.error);
        return 1;
    }
    const config::ManusConfig& cfg = *loaded.config;

    // Fail fast without a PIN: never expose the injection surface unauthorised.
    auto stored = auth::load_pairing(root);
    if (!stored.pin) {
        logger.error(stored.error);
        return 1;
    }
    auth::Pairing pairing(std::move(*stored.pin), std::move(stored.secret), cfg.token_ttl_hours, [] {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    });

    const std::unique_ptr<inject::InputSink> sink = inject::make_platform_input_sink();
    if (!sink->available()) {
        logger.warn("[manus] このプラットフォームでは入力を注入できません (helper down)。注入は helper_down で失敗します");
    }

    asio::io_context io(1);
    app::ManusApp application(*sink, pairing, logger);
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
    logger.info("[manus] listening on http://" + cfg.bind + ":" + std::to_string(cfg.port) +
                (sink->available() ? " (helper up)" : " (helper down)"));

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
                return run_set_pin(resolve_root(command), command.pin);
            case app::CommandKind::serve:
                return run_service(resolve_root(command));
        }
    } catch (const std::exception& ex) {
        std::cerr << "[fatal] " << ex.what() << std::endl;
        return 1;
    }
    return 1;
}
