#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/http/request.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
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
    if (!resp.has_value())
        GTEST_SKIP() << "no external network connectivity";
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
    scope.server._webservice->set_entry_point(
        "res",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Patch);
    scope.server._webservice->set_entry_point(
        "res",
        [](const HttpRequest &, HttpResponse & resp) { resp.set_plain_content("ok"); },
        HttpRequest::Head);
    scope.server._webservice->set_entry_point(
        "opts",
        [](const HttpRequest &, HttpResponse & resp) { resp.set_plain_content("options-ok"); },
        HttpRequest::Options);
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

    RequestOptions cors;
    cors.headers["Origin"] = "https://app.com";
    cors.headers["Access-Control-Request-Method"] = "POST";
    EXPECT_EQ(http::options("localhost:3004/api/res", cors)->status(), HttpStatus::NoContent);
    {
        auto r = http::patch("localhost:3004/api/res", "patched");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->content().cpp_str(), "patched");
    }
    {
        auto r = http::head("localhost:3004/api/res");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
    }
    {
        // a preflight is an OPTIONS carrying origin and access-control-request-method
        auto r = http::options("localhost:3004/api/opts", cors);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::NoContent);
        EXPECT_EQ(r->http_header().find("access-control-allow-methods"),
                  "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS");
    }
    {
        auto r = http::get("localhost:3004/api/opts", cors);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->http_header().find("access-control-allow-origin"), "https://app.com");
    }
    {
        // a plain OPTIONS reaches the routes
        auto r = http::options("localhost:3004/api/opts");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Ok);
        EXPECT_EQ(r->content().cpp_str(), "options-ok");
    }
    {
        // the path is known, the method is not: the answer carries what is allowed
        auto r = http::options("localhost:3004/api/res");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::MethodNotAllowed);
        EXPECT_EQ(r->http_header().find("allow"), "GET, POST, DELETE, PATCH, HEAD");
    }
    {
        // the same answer is reached through the body path
        auto r = http::post("localhost:3004/api/opts", "body");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::MethodNotAllowed);
        EXPECT_EQ(r->http_header().find("allow"), "OPTIONS");
    }
    {
        // nothing routes that path
        auto r = http::options("localhost:3004/api/missing");
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::NotFound);
    }
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
    RequestOptions bad;
    bad.username = "admin";
    bad.password = "wrong";
    {
        auto r = http::get("localhost:3004/api/who", bad);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->status(), HttpStatus::Unauthorized);
    }

    // basic auth → 200, user propagated
    RequestOptions basic;
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
    RequestOptions token;
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
    scope.server._webservice->set_entry_point("fs/{path...}", [](const HttpRequest & req, HttpResponse & resp) {
        resp.set_plain_content(std::string(req.path_param("path").value_or("")));
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

TEST_F(TestRequest, test_http_status)
{
    EXPECT_EQ(HttpStatus::to_string(200), "OK");
    EXPECT_EQ(HttpStatus::to_string(404), "Not Found");
    EXPECT_EQ(HttpStatus::to_string(500), "Internal Server Error");
    EXPECT_EQ(HttpStatus::to_string(429), "Too Many Requests");
    EXPECT_EQ(HttpStatus::to_string(999), "Unknown");

    EXPECT_TRUE(HttpStatus::is_redirect(301));
    EXPECT_TRUE(HttpStatus::is_redirect(302));
    EXPECT_TRUE(HttpStatus::is_redirect(307));
    EXPECT_TRUE(HttpStatus::is_redirect(308));
    EXPECT_FALSE(HttpStatus::is_redirect(200));

    EXPECT_TRUE(HttpStatus::is_post_downgrade(301));
    EXPECT_TRUE(HttpStatus::is_post_downgrade(303));
    EXPECT_FALSE(HttpStatus::is_post_downgrade(307));

    EXPECT_TRUE(HttpStatus::is_rate_limit(429));
    EXPECT_TRUE(HttpStatus::is_rate_limit(503));
    EXPECT_FALSE(HttpStatus::is_rate_limit(200));
}

// free helpers are one-shots: each call opens its own connection
TEST_F(TestRequest, test_stateless_connections)
{
    ServerScope scope;
    int get_count = 0;
    scope.server._webservice->set_entry_point("counter", [&get_count](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("hit-" + std::to_string(++get_count));
    });
    scope.start(3005);

    auto r1 = http::get("localhost:3005/api/counter");
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->content().cpp_str(), "hit-1");

    auto r2 = http::get("localhost:3005/api/counter");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->content().cpp_str(), "hit-2");
    EXPECT_EQ(get_count, 2);
}

TEST_F(TestRequest, test_put_file)
{
    ServerScope scope;
    sihd::sys::TmpDir tmp;
    ASSERT_TRUE(tmp);

    scope.server._webservice->set_entry_point(
        "upload",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Put);
    scope.start(3005);

    // larger than one curl read buffer: the file must stream through the read callback
    std::string content(256 * 1024, 'x');
    for (size_t i = 0; i < content.size(); i += 16)
        std::memcpy(content.data() + i, "0123456789abcdef", 16);

    const std::string path = sihd::sys::fs::combine(tmp.path(), "put.bin");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content.data(), (std::streamsize)content.size());
    }

    auto resp = http::put("localhost:3005/api/upload", path);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), 200u);
    EXPECT_EQ(resp->content().cpp_str(), content);

    EXPECT_FALSE(http::put("localhost:3005/api/upload", "/no/such/file/here").has_value());
}

// a response cut short by max_response_size is still returned, truncated
TEST_F(TestRequest, test_max_response_size_overflow)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("big", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(std::string(8192, 'x'));
    });
    scope.start(3005);

    RequestOptions options;
    options.max_response_size = 1024;
    auto r = http::get("localhost:3005/api/big", options);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->status(), HttpStatus::Ok);
    EXPECT_LE(r->content().size(), 1024u);

    // without the limit the whole body comes through
    auto full = http::get("localhost:3005/api/big");
    ASSERT_TRUE(full.has_value());
    EXPECT_EQ(full->content().size(), 8192u);
}

// query parameters reach the handler and the progress callback observes the transfer
TEST_F(TestRequest, test_request_options_parameters_and_progress)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point(
        "params",
        [](const HttpRequest & req, HttpResponse & resp) {
            resp.set_plain_content(req.query_param("q").value_or("missing"));
        },
        HttpRequest::Post);
    scope.start(3005);

    RequestOptions options;
    options.parameters["q"] = "sihd";
    bool saw_upload_total = false;
    bool saw_download_total = false;
    options.progress = [&](const http::Progress & prog) {
        saw_upload_total = saw_upload_total || prog.upload_total == 4;
        saw_download_total = saw_download_total || prog.download_total > 0;
        return true;
    };

    auto r = http::post("localhost:3005/api/params", "body", options);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->status(), HttpStatus::Ok);
    EXPECT_EQ(r->content().cpp_str(), "sihd");
    EXPECT_TRUE(saw_upload_total);
    EXPECT_TRUE(saw_download_total);
}

} // namespace test
