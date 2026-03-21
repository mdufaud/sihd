#include <gtest/gtest.h>

#include <sihd/http/CurlOptions.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/Logger.hpp>

#include "http_test_helpers.hpp"

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd;
using namespace sihd::util;
using namespace sihd::http;

class TestRequest: public ::testing::Test
{
    protected:
        TestRequest() { sihd::util::LoggerManager::stream(); }
        virtual ~TestRequest() { sihd::util::LoggerManager::clear_loggers(); }
};

TEST_F(TestRequest, test_http_get_external)
{
    auto resp = http::get("https://www.google.com");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), 200u);
    EXPECT_GT(resp->content().size(), 0u);
}

TEST_F(TestRequest, test_invalid_url)
{
    auto resp = http::get("http://localhost:19999/no-server-here");
    EXPECT_FALSE(resp.has_value());
}

// GET, POST, PUT, DELETE and CORS OPTIONS all work via the free functions
TEST_F(TestRequest, test_http_methods)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("res", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });
    scope.server._webservice->set_entry_point(
        "res",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Post);
    scope.server._webservice->set_entry_point(
        "res",
        [](const HttpRequest &, HttpResponse & resp) { resp.set_status(HttpStatus::Ok); },
        HttpRequest::Delete);
    scope.server.set_cors_origin("https://app.com");
    scope.start(3004);

    {
        auto r = http::get("localhost:3004/api/res");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
    }
    {
        auto r = http::post("localhost:3004/api/res", "body");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->content().cpp_str(), "body");
    }
    {
        auto r = http::del("localhost:3004/api/res");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
    }

    CurlOptions cors;
    cors.headers["Origin"] = "https://app.com";
    cors.headers["Access-Control-Request-Method"] = "POST";
    EXPECT_EQ(http::options("localhost:3004/api/res", cors)->status(), HttpStatus::NoContent);
}

// async_get, async_post, parallel fan-out all work correctly
TEST_F(TestRequest, test_async_requests)
{
    ServerScope scope;
    std::atomic<int> calls {0};
    scope.server._webservice->set_entry_point(
        "echo",
        [&](const HttpRequest & req, HttpResponse & resp) {
            ++calls;
            resp.set_plain_content(req.content().cpp_str());
        },
        HttpRequest::Post);
    scope.server._webservice->set_entry_point("ping", [&](const HttpRequest &, HttpResponse & resp) {
        ++calls;
        resp.set_plain_content("pong");
    });
    scope.start(3004);

    auto f1 = http::async_get("localhost:3004/api/ping");
    auto f2 = http::async_post("localhost:3004/api/echo", "hello");
    EXPECT_EQ(f1.get()->content().cpp_str(), "pong");
    EXPECT_EQ(f2.get()->content().cpp_str(), "hello");

    // fan-out: 10 concurrent GETs all succeed
    constexpr int n = 10;
    std::vector<FutureHttpResponse> futures;
    futures.reserve(n);
    for (int i = 0; i < n; ++i)
        futures.push_back(http::async_get("localhost:3004/api/ping"));
    for (auto & f : futures)
        EXPECT_EQ(f.get()->status(), HttpStatus::Ok);
    EXPECT_EQ(calls.load(), 2 + n);
}

// basic-auth and bearer-token enforce access and propagate identity to the handler
TEST_F(TestRequest, test_auth)
{
    ServerScope scope;
    TestAuth auth;
    std::string captured_user, captured_token;
    scope.server.set_authenticator(&auth);
    scope.server._webservice->set_entry_point("who", [&](const HttpRequest & req, HttpResponse & resp) {
        captured_user = req.auth_user();
        captured_token = req.auth_token();
        resp.set_plain_content("ok");
    });
    scope.start(3004);

    // no credentials → 401
    {
        auto r = http::get("localhost:3004/api/who");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Unauthorized);
    }

    // wrong password → 401
    CurlOptions bad;
    bad.username = "admin";
    bad.password = "wrong";
    {
        auto r = http::get("localhost:3004/api/who", bad);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Unauthorized);
    }

    // basic auth → 200, user propagated
    CurlOptions basic;
    basic.username = "admin";
    basic.password = "secret";
    {
        auto r = http::get("localhost:3004/api/who", basic);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
    }
    EXPECT_EQ(captured_user, "admin");
    EXPECT_TRUE(captured_token.empty());

    // bearer token → 200, token propagated
    captured_user.clear();
    CurlOptions token;
    token.token = "my-secret-token-123";
    {
        auto r = http::get("localhost:3004/api/who", token);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
    }
    EXPECT_TRUE(captured_user.empty());
    EXPECT_EQ(captured_token, "my-secret-token-123");
}

// route matching: path params, catchall, specificity (literal beats {param}), 404 on miss
TEST_F(TestRequest, test_routing)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("items/{id}", [](const HttpRequest & req, HttpResponse & resp) {
        resp.set_plain_content(std::string(req.path_param("id").value_or("")));
    });
    scope.server._webservice->set_entry_point("items/special", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("literal");
    });
    scope.server._webservice->set_entry_point("fs/{path...}",
                                              [](const HttpRequest & req, HttpResponse & resp) {
                                                  resp.set_plain_content(
                                                      std::string(req.path_param("path").value_or("")));
                                              });
    scope.start(3004);

    // parameterized route
    {
        auto r = http::get("localhost:3004/api/items/42");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->content().cpp_str(), "42");
    }
    // literal beats {id}
    {
        auto r = http::get("localhost:3004/api/items/special");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->content().cpp_str(), "literal");
    }
    // catchall
    {
        auto r = http::get("localhost:3004/api/fs/a/b/c.txt");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->content().cpp_str(), "a/b/c.txt");
    }
    // unmatched route → 404
    {
        auto r = http::get("localhost:3004/api/missing");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::NotFound);
    }
}

} // namespace test
