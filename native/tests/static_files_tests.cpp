// src/http/static-server.ts: path resolution, MIME, ETag/304, no-store, 403/404/500.
#include <filesystem>
#include <string>

#include "http/static_files.hpp"
#include "http/static_server.hpp"
#include "log/logger.hpp"
#include "temp_dir.hpp"
#include "test_harness.hpp"

using namespace manus::http;
using manus::test::TempDir;

namespace {

std::string header(const HttpReply& r, const std::string& name) {
    for (const auto& h : r.headers) {
        if (h.name == name) return h.value;
    }
    return "<none>";
}

struct PublicDir {
    PublicDir() {
        root.write("public/index.html", "<!doctype html>index");
        root.write("public/style.css", "body{}");
        root.write("public/js/main.js", "console.log(1)");
        root.write("public/js/main.js.map", "{}");
        root.write("public/icon.svg", "<svg/>");
        root.write("public-evil/x.js", "evil");
        root.write("secret.txt", "secret");
    }
    std::filesystem::path pub() const { return root.path() / "public"; }
    HttpReply get(std::string_view pathname, std::optional<std::string_view> inm = std::nullopt) {
        HttpRequest req;
        req.method = "GET";
        req.target = pathname;
        req.if_none_match = inm;
        return serve_static(req, pathname, pub(), logger);
    }
    TempDir root;
    manus::log::NullLogger logger;
};

}  // namespace

TEST_CASE(extname_matches_node) {
    // Values from Node 22 path.extname / path.win32.extname.
    const std::pair<const char*, const char*> cases[] = {
        {"/foo.bar/", ".bar"}, {"/.env", ""},  {"/...", "."},       {"/index.", "."},          {"/a/.b.c", ".c"},
        {"/..", ""},           {"/.", ""},     {"/x..", "."},       {"/foo.bar//", ".bar"},    {"/.git/config", ""},
        {"/a.b/c", ""},        {"/js/main.js", ".js"}, {"/a\\b.css", ".css"}, {"/", ""},
    };
    for (const auto& [path, ext] : cases) CHECK_EQ(std::string(node_extname(path)), std::string(ext));
}

TEST_CASE(mime_table) {
    CHECK_EQ(mime_type_for(".html"), std::string("text/html; charset=utf-8"));
    CHECK_EQ(mime_type_for(".js"), std::string("text/javascript; charset=utf-8"));
    CHECK_EQ(mime_type_for(".css"), std::string("text/css; charset=utf-8"));
    CHECK_EQ(mime_type_for(".json"), std::string("application/json; charset=utf-8"));
    CHECK_EQ(mime_type_for(".svg"), std::string("image/svg+xml"));
    CHECK_EQ(mime_type_for(".png"), std::string("image/png"));
    CHECK_EQ(mime_type_for(".webmanifest"), std::string("application/manifest+json"));
    CHECK_EQ(mime_type_for(".map"), std::string("application/octet-stream"));
    CHECK_EQ(mime_type_for(".HTML"), std::string("application/octet-stream"));
}

TEST_CASE(etag_shape) {
    CHECK_EQ(make_etag(0, 0), std::string("\"0-0\""));
    CHECK_EQ(make_etag(35, 36), std::string("\"z-10\""));
}

TEST_CASE(resolution_rules) {
    const std::filesystem::path root = "/srv/public";
    auto kind = [&](std::string_view p) { return resolve_static_path(p, root).kind; };
    CHECK(kind("/") == StaticKind::file);
    CHECK(resolve_static_path("/", root).path == root / "index.html");
    CHECK(resolve_static_path("/keyboard", root).path == root / "index.html");  // no extension -> SPA
    CHECK(resolve_static_path("/js/..%2fstyle.css", root).path == root / "style.css");
    CHECK(kind("/..%2fsecret.txt") == StaticKind::forbidden);
    CHECK(kind("/..%2fpublic-evil/x.js") == StaticKind::forbidden);  // Node's prefix check let this through
    CHECK(kind("/js/..%2f..%2fsecret.txt") == StaticKind::forbidden);
    CHECK(kind("/..%5csecret.txt") == StaticKind::forbidden);        // backslash is a separator (Windows)
    CHECK(kind("/%E0%A4%A") == StaticKind::bad_uri);
    CHECK(kind("/%FF.js") == StaticKind::bad_uri);
    CHECK(kind("/a%00b.js") == StaticKind::not_found);
    CHECK(kind("/index.html::$DATA") == StaticKind::not_found);  // NTFS stream
    CHECK(kind("/C:%5cWindows%5cwin.ini") == StaticKind::not_found);
    CHECK(kind("/con.js") == StaticKind::not_found);
    CHECK(kind("/js/NUL.txt") == StaticKind::not_found);
    CHECK(kind("/com1.css") == StaticKind::not_found);
    CHECK(kind("/console.js") == StaticKind::file);
    CHECK(kind("/foo.bar/") == StaticKind::not_found);  // trailing separator never names a file
    CHECK(resolve_static_path("/js/main.js.map", root).mime == "application/octet-stream");
}

TEST_CASE(serves_files_with_no_store_and_etag) {
    PublicDir d;
    const auto r = d.get("/js/main.js");
    CHECK_EQ(r.status, 200u);
    CHECK_EQ(header(r, "content-type"), std::string("text/javascript; charset=utf-8"));
    CHECK_EQ(header(r, "cache-control"), std::string("no-store, no-cache, must-revalidate"));
    CHECK(header(r, "etag").front() == '"');
    REQUIRE(r.file.has_value());
    CHECK_EQ(r.file_size, std::uint64_t{14});
    const auto index = d.get("/");
    REQUIRE(index.file.has_value());
    CHECK(index.file->filename() == "index.html");
    CHECK_EQ(header(index, "content-type"), std::string("text/html; charset=utf-8"));
}

TEST_CASE(if_none_match_gives_304) {
    PublicDir d;
    const std::string etag = header(d.get("/style.css"), "etag");
    const auto r = d.get("/style.css", etag);
    CHECK_EQ(r.status, 304u);
    CHECK_EQ(header(r, "etag"), etag);
    CHECK_EQ(header(r, "cache-control"), std::string("no-store, no-cache, must-revalidate"));
    CHECK(!r.file.has_value());
    CHECK_EQ(d.get("/style.css", std::string_view("\"other\"")).status, 200u);
}

TEST_CASE(errors_have_plain_bodies_without_content_type) {
    PublicDir d;
    auto r = d.get("/missing.js");
    CHECK_EQ(r.status, 404u);
    CHECK_EQ(r.body, std::string("not found"));
    CHECK_EQ(header(r, "content-type"), std::string("<none>"));
    r = d.get("/js");  // directory, no extension -> index.html
    CHECK_EQ(r.status, 200u);
    r = d.get("/js.dir");
    CHECK_EQ(r.status, 404u);
    r = d.get("/..%2fsecret.txt");
    CHECK_EQ(r.status, 403u);
    CHECK_EQ(r.body, std::string("forbidden"));
    r = d.get("/%E0%A4%A");
    CHECK_EQ(r.status, 500u);
    CHECK_EQ(r.body, std::string("internal error"));
}

TEST_CASE(directories_named_like_files_are_404) {
    PublicDir d;
    d.root.write("public/dir.js/inner.txt", "x");
    CHECK_EQ(d.get("/dir.js").status, 404u);
}

TEST_CASE(file_size_limit_exact_and_plus_one) {
    PublicDir d;
    d.root.write("public/big.png", "");
    std::filesystem::resize_file(d.pub() / "big.png", kMaxStaticFileBytes);  // sparse on Linux
    CHECK_EQ(d.get("/big.png").status, 200u);
    std::filesystem::resize_file(d.pub() / "big.png", kMaxStaticFileBytes + 1);
    CHECK_EQ(d.get("/big.png").status, 500u);
}
