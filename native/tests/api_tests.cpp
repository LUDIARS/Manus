// src/http/api.ts behaviour: /health and POST /api/pair, statuses and JSON bodies.
#include <string>

#include "auth/pairing.hpp"
#include "http/api.hpp"
#include "test_harness.hpp"

using namespace manus;

namespace {

struct ApiFixture {
    ApiFixture()
        : pairing(auth::StoredPin{"00112233445566778899aabbccddeeff",
                                  auth::hash_pin("123456", "00112233445566778899aabbccddeeff")},
                  std::string(32, '\x01'), 24, [this] { return now; }) {
        context.pairing = &pairing;
        context.helper_up = true;
    }
    std::optional<http::HttpReply> call(std::string_view method, std::string_view path, std::string_view body = "") {
        http::HttpRequest request;
        request.method = method;
        request.target = path;
        request.body = body;
        return http::handle_api(request, path, context);
    }
    std::int64_t now = 1'700'000'000'000;
    auth::Pairing pairing;
    http::ApiContext context;
};

std::string content_type(const http::HttpReply& r) {
    for (const auto& h : r.headers) {
        if (h.name == "content-type") return h.value;
    }
    return "<none>";
}

}  // namespace

TEST_CASE(health_reports_helper_state_for_any_method) {
    ApiFixture f;
    auto r = f.call("GET", "/health");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200u);
    CHECK_EQ(r->body, std::string(R"({"ok":true,"service":"manus","helper":"up"})"));
    CHECK_EQ(content_type(*r), std::string("application/json; charset=utf-8"));
    f.context.helper_up = false;
    r = f.call("POST", "/health");
    REQUIRE(r.has_value());
    CHECK_EQ(r->body, std::string(R"({"ok":true,"service":"manus","helper":"down"})"));
}

TEST_CASE(pair_success_returns_token) {
    ApiFixture f;
    const auto r = f.call("POST", "/api/pair", R"({"pin":"123456"})");
    REQUIRE(r.has_value());
    CHECK_EQ(r->status, 200u);
    CHECK(r->body.rfind(R"({"token":"1700086400000.)", 0) == 0);
}

TEST_CASE(pair_bad_requests_are_400) {
    ApiFixture f;
    for (const char* body : {"", "not json", "[]", "null", "\"123456\"", "{}", R"({"pin":123456})", R"({"pin":null})",
                             R"({"PIN":"123456"})", R"({"pin":["123456"]})"}) {
        const auto r = f.call("POST", "/api/pair", body);
        REQUIRE(r.has_value());
        CHECK_EQ(r->status, 400u);
        CHECK_EQ(r->body, std::string(R"({"error":"bad_request"})"));
    }
    // Bad requests are not pairing attempts: the PIN still works afterwards.
    CHECK_EQ(f.call("POST", "/api/pair", R"({"pin":"123456"})")->status, 200u);
}

TEST_CASE(pair_extra_fields_and_duplicate_keys_like_json_parse) {
    ApiFixture f;
    CHECK_EQ(f.call("POST", "/api/pair", R"({"pin":"123456","x":1})")->status, 200u);
    CHECK_EQ(f.call("POST", "/api/pair", R"({"pin":"000000","pin":"123456"})")->status, 200u);
}

TEST_CASE(pair_wrong_pin_401_then_locked_429) {
    ApiFixture f;
    for (int i = 0; i < 5; ++i) {
        const auto r = f.call("POST", "/api/pair", R"({"pin":"000000"})");
        CHECK_EQ(r->status, 401u);
        CHECK_EQ(r->body, std::string(R"({"error":"bad_pin"})"));
    }
    const auto r = f.call("POST", "/api/pair", R"({"pin":"123456"})");
    CHECK_EQ(r->status, 429u);
    CHECK_EQ(r->body, std::string(R"({"error":"locked"})"));
}

TEST_CASE(pair_other_methods_fall_through_to_static) {
    ApiFixture f;
    CHECK(!f.call("GET", "/api/pair").has_value());
    CHECK(!f.call("PUT", "/api/pair", R"({"pin":"123456"})").has_value());
    CHECK(!f.call("POST", "/api/pairs", R"({"pin":"123456"})").has_value());
    CHECK(!f.call("GET", "/").has_value());
}

TEST_CASE(pair_body_json_limits) {
    ApiFixture f;
    std::string deep = R"({"pin":"123456","x":)";
    deep += std::string(40, '[') + std::string(40, ']') + "}";
    CHECK_EQ(f.call("POST", "/api/pair", deep)->status, 400u);  // deeper than 32
    std::string many = R"({"pin":"123456","x":[)";
    for (int i = 0; i < 600; ++i) many += (i ? ",0" : "0");
    many += "]}";
    CHECK_EQ(f.call("POST", "/api/pair", many)->status, 400u);  // more than 512 elements
}
