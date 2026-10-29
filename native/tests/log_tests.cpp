// JSONL log (Vestigium file format §2: <logs>/<service>/YYYY-MM-DD.jsonl, one JSON object per line).
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "log/jsonl_logger.hpp"
#include "log/jsonl_record.hpp"
#include "temp_dir.hpp"
#include "test_harness.hpp"

using namespace manus::log;

TEST_CASE(record_has_vestigium_keys_in_order) {
    JsonlRecord r;
    r.ts = 1779843000123;
    r.level = Level::warn;
    r.service = "manus";
    r.msg = "line1\nline2";
    r.pid = 42;
    CHECK_EQ(format_jsonl_record(r),
             std::string(R"({"ts":1779843000123,"level":"warn","service":"manus","channel":"stderr","msg":"line1\nline2","pid":42})"
                         "\n"));
    r.level = Level::info;
    CHECK(format_jsonl_record(r).find(R"("channel":"app")") != std::string::npos);
}

TEST_CASE(record_truncates_long_messages_at_64k) {
    JsonlRecord r;
    r.service = "manus";
    const std::string exact(kMaxMessageBytes, 'a');
    r.msg = exact;
    auto parsed = nlohmann::json::parse(format_jsonl_record(r));
    CHECK_EQ(parsed["msg"].get<std::string>().size(), kMaxMessageBytes);

    const std::string over(kMaxMessageBytes + 1, 'a');
    r.msg = over;
    parsed = nlohmann::json::parse(format_jsonl_record(r));
    const std::string msg = parsed["msg"].get<std::string>();
    CHECK(msg.size() < over.size());
    CHECK(msg.find("(truncated 33)") != std::string::npos);
}

TEST_CASE(record_replaces_invalid_utf8) {
    JsonlRecord r;
    r.service = "manus";
    r.msg = "bad \xff byte";
    const auto line = format_jsonl_record(r);
    CHECK(nlohmann::json::accept(line.substr(0, line.size() - 1)));
}

TEST_CASE(utc_day_and_file_names) {
    CHECK_EQ(utc_day(0), std::string("1970-01-01"));
    CHECK_EQ(utc_day(1779843000123), std::string("2026-05-27"));
    CHECK_EQ(parse_day_file_name("1970-01-02.jsonl").value_or(-1), std::int64_t{1});
    CHECK_EQ(parse_day_file_name("1970-01-02.jsonl.gz").value_or(-1), std::int64_t{1});
    CHECK(!parse_day_file_name("1970-13-02.jsonl").has_value());
    CHECK(!parse_day_file_name("notes.txt").has_value());
    CHECK(!parse_day_file_name("1970-01-02.jsonl.bak").has_value());
}

TEST_CASE(logger_writes_jsonl_file_and_sweeps_old_days) {
    manus::test::TempDir dir;
    const auto service_dir = dir.path() / "manus";
    dir.write("manus/2000-01-01.jsonl", "{}\n");       // far older than retention: removed
    dir.write("manus/notes.txt", "keep");              // not a day file: kept
    {
        JsonlLogger logger({"manus", dir.path(), 14, false, true});
        logger.info("[manus] listening");
        logger.error("[fatal] boom");
    }
    CHECK(!std::filesystem::exists(service_dir / "2000-01-01.jsonl"));
    CHECK(std::filesystem::exists(service_dir / "notes.txt"));

    int lines = 0;
    for (const auto& entry : std::filesystem::directory_iterator(service_dir)) {
        const auto name = entry.path().filename().string();
        if (!parse_day_file_name(name)) continue;
        std::ifstream in(entry.path());
        std::string line;
        while (std::getline(in, line)) {
            const auto j = nlohmann::json::parse(line);
            CHECK_EQ(j["service"].get<std::string>(), std::string("manus"));
            ++lines;
        }
    }
    CHECK_EQ(lines, 2);
}

TEST_CASE(logger_survives_unwritable_directory) {
    manus::test::TempDir dir;
    dir.write("blocker", "a file where the logs directory should be");
    JsonlLogger logger({"manus", dir.path() / "blocker", 14, false, true});
    logger.info("does not throw");  // reported once on stderr, the service keeps running
    CHECK(true);
}
