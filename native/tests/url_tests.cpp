// Expected values were taken from Node 22: new URL(t, "http://localhost").pathname,
// .searchParams.get("token") and decodeURIComponent(s).
#include <optional>
#include <string>

#include "http/url.hpp"
#include "test_harness.hpp"

using manus::http::decode_uri_component;
using manus::http::query_param;
using manus::http::request_pathname;

TEST_CASE(pathname_matches_whatwg_url) {
    const std::pair<const char*, const char*> cases[] = {
        {"/", "/"},
        {"/health", "/health"},
        {"/health?x=1", "/health"},
        {"/ws?token=abc", "/ws"},
        {"/a/%2e%2E/b/./c/%2e/", "/b/c/"},
        {"/a\\b/../c", "/a/c"},
        {"//evil.com/x?token=1", "/x"},
        {"/../package.json", "/package.json"},
        {"/%2e%2e/secret.txt", "/secret.txt"},
        {"/..%2fsecret.txt", "/..%2fsecret.txt"},
        {"/js/..%2fstyle.css", "/js/..%2fstyle.css"},
        {"/a/..", "/"},
        {"/a/.", "/a/"},
        {"/a/../", "/"},
        {"http://host/p/q", "/p/q"},
        {"/ws#frag", "/ws"},
        {"*", "/*"},
        {"/x/%2E", "/x/"},
        {"/ws/..", "/"},
        {"/w%73", "/w%73"},
    };
    for (const auto& [target, expected] : cases) {
        CHECK_EQ(request_pathname(target), std::string(expected));
    }
}

TEST_CASE(pathname_reads_raw_high_bytes_as_latin1_like_node) {
    // Node decodes the request line as Latin-1, so the UTF-8 bytes of "あ" become three characters.
    CHECK_EQ(request_pathname("/\xE3\x81\x82"), std::string("/%C3%A3%C2%81%C2%82"));
}

TEST_CASE(query_param_matches_url_search_params) {
    CHECK_EQ(query_param("/ws?token=a+b%20c&token=2", "token").value_or("<null>"), std::string("a b c"));
    CHECK_EQ(query_param("/ws?x=1&token=", "token").value_or("<null>"), std::string(""));
    CHECK_EQ(query_param("/ws?token", "token").value_or("<null>"), std::string(""));
    CHECK_EQ(query_param("/ws?tok%65n=v", "token").value_or("<null>"), std::string("v"));
    CHECK_EQ(query_param("/ws?token=%zz", "token").value_or("<null>"), std::string("%zz"));
    CHECK(!query_param("/ws?a=1#token=2", "token").has_value());
    CHECK(!query_param("/ws", "token").has_value());
}

TEST_CASE(decode_uri_component_matches_js) {
    CHECK_EQ(decode_uri_component("/a%20b").value_or("<URIError>"), std::string("/a b"));
    CHECK_EQ(decode_uri_component("%E3%81%82").value_or("<URIError>"), std::string("\xE3\x81\x82"));
    CHECK_EQ(decode_uri_component("abc%2Fdef").value_or("<URIError>"), std::string("abc/def"));
    for (const char* bad : {"%zz", "%E0%A4%A", "%FF", "%C0%80", "%ED%A0%80", "%F4%90%80%80", "%", "%4"}) {
        CHECK(!decode_uri_component(bad).has_value());
    }
}

TEST_CASE(decode_uri_component_never_grows) {
    const std::string encoded = std::string(3000, 'a') + "%41";
    const auto decoded = decode_uri_component(encoded);
    REQUIRE(decoded.has_value());
    CHECK(decoded->size() <= encoded.size());
}
