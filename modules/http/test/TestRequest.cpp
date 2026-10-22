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

using enum sihd::util::ErrorCode;

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd;
using namespace sihd::util;
using namespace sihd::http;

class TestRequest: public ::testing::Test
{
    protected:
        TestRequest() { LoggerManager::stream(); }
        virtual ~TestRequest() { LoggerManager::clear_loggers(); }
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
    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().code, io_error);
    EXPECT_FALSE(resp.error().message.empty());
}

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
    scope.start(shard_port(3004));

    {
        auto response = http::get(shard_url("localhost:3004/api/res"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
    }

    {
        auto response = http::post(shard_url("localhost:3004/api/res"), "body");
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "body");
    }

    {
        auto response = http::del(shard_url("localhost:3004/api/res"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
    }

    RequestOptions cors;
    cors.headers["Origin"] = "https://app.com";
    cors.headers["Access-Control-Request-Method"] = "POST";

    EXPECT_EQ(http::options(shard_url("localhost:3004/api/res"), cors)->status(), HttpStatus::NoContent);

    {
        auto response = http::patch(shard_url("localhost:3004/api/res"), "patched");
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "patched");
    }

    {
        auto response = http::head(shard_url("localhost:3004/api/res"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
    }

    {
        // a preflight is an OPTIONS carrying origin and access-control-request-method
        auto response = http::options(shard_url("localhost:3004/api/opts"), cors);
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::NoContent);
        EXPECT_EQ(response->http_header().find("access-control-allow-methods"),
                  "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS");
    }

    {
        auto response = http::get(shard_url("localhost:3004/api/opts"), cors);
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->http_header().find("access-control-allow-origin"), "https://app.com");
    }

    {
        auto response = http::options(shard_url("localhost:3004/api/opts"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
        EXPECT_EQ(response->content().cpp_str(), "options-ok");
    }

    {
        // the path is known, the method is not: the answer carries what is allowed
        auto response = http::options(shard_url("localhost:3004/api/res"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::MethodNotAllowed);
        EXPECT_EQ(response->http_header().find("allow"), "GET, POST, DELETE, PATCH, HEAD");
    }

    {
        auto response = http::post(shard_url("localhost:3004/api/opts"), "body");
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::MethodNotAllowed);
        EXPECT_EQ(response->http_header().find("allow"), "OPTIONS");
    }

    {
        auto response = http::options(shard_url("localhost:3004/api/missing"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::NotFound);
    }
}

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
    scope.start(shard_port(3004));

    auto ping_future = http::async_get(shard_url("localhost:3004/api/ping"));
    auto echo_future = http::async_post(shard_url("localhost:3004/api/echo"), "hello");
    EXPECT_EQ(ping_future.get()->content().cpp_str(), "pong");
    EXPECT_EQ(echo_future.get()->content().cpp_str(), "hello");

    constexpr int request_count = 10;
    std::vector<FutureHttpResponse> futures;
    futures.reserve(request_count);
    for (int i = 0; i < request_count; ++i)
        futures.push_back(http::async_get(shard_url("localhost:3004/api/ping")));
    for (auto & future : futures)
        EXPECT_EQ(future.get()->status(), HttpStatus::Ok);
    EXPECT_EQ(calls.load(), 2 + request_count);
}

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
    scope.start(shard_port(3004));

    {
        auto response = http::get(shard_url("localhost:3004/api/who"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Unauthorized);
    }

    RequestOptions wrong_credentials;
    wrong_credentials.username = "admin";
    wrong_credentials.password = "wrong";
    {
        auto response = http::get(shard_url("localhost:3004/api/who"), wrong_credentials);
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Unauthorized);
    }

    RequestOptions basic;
    basic.username = "admin";
    basic.password = "secret";
    {
        auto response = http::get(shard_url("localhost:3004/api/who"), basic);
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
    }
    EXPECT_EQ(captured_user, "admin");
    EXPECT_TRUE(captured_token.empty());

    captured_user.clear();
    RequestOptions token;
    token.token = "my-secret-token-123";
    {
        auto response = http::get(shard_url("localhost:3004/api/who"), token);
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::Ok);
    }
    EXPECT_TRUE(captured_user.empty());
    EXPECT_EQ(captured_token, "my-secret-token-123");
}

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
    scope.start(shard_port(3004));

    {
        auto response = http::get(shard_url("localhost:3004/api/items/42"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "42");
    }

    {
        auto response = http::get(shard_url("localhost:3004/api/items/special"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "literal");
    }

    {
        auto response = http::get(shard_url("localhost:3004/api/fs/a/b/c.txt"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "a/b/c.txt");
    }

    {
        auto response = http::get(shard_url("localhost:3004/api/missing"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->status(), HttpStatus::NotFound);
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
    scope.start(shard_port(3005));

    auto first_response = http::get(shard_url("localhost:3005/api/counter"));
    ASSERT_TRUE(first_response.has_value());
    EXPECT_EQ(first_response->content().cpp_str(), "hit-1");

    auto second_response = http::get(shard_url("localhost:3005/api/counter"));
    ASSERT_TRUE(second_response.has_value());
    EXPECT_EQ(second_response->content().cpp_str(), "hit-2");
    EXPECT_EQ(get_count, 2);
}

namespace
{

void capture_post_head(std::string & request_head, const RequestOptions & options)
{
    const std::string body(2 * 1024 * 1024, 'x');
    HeadCaptureServer capture(body.size());
    ASSERT_TRUE(capture.start());

    auto resp = http::post(fmt::format("127.0.0.1:{}/upload", capture.port()), body, options);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    request_head = capture.finish();
    ASSERT_FALSE(request_head.empty());
}

} // namespace

TEST_F(TestRequest, test_post_large_body_sends_no_expect_header)
{
    std::string request_head;
    capture_post_head(request_head, RequestOptions::none());
    EXPECT_EQ(request_head.find("Expect"), std::string::npos);
}

TEST_F(TestRequest, test_post_keeps_caller_expect_header)
{
    RequestOptions options;
    options.headers["Expect"] = "100-continue";
    std::string request_head;
    capture_post_head(request_head, options);
    EXPECT_NE(request_head.find("Expect: 100-continue"), std::string::npos);
}

TEST_F(TestRequest, test_put_file)
{
    ServerScope scope;
    sihd::sys::TmpDir dir;
    ASSERT_TRUE(dir);

    scope.server._webservice->set_entry_point(
        "upload",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Put);
    scope.start(shard_port(3005));

    // larger than one curl read buffer: the file must stream through the read callback
    std::string content(256 * 1024, 'x');
    for (size_t i = 0; i < content.size(); i += 16)
        std::memcpy(content.data() + i, "0123456789abcdef", 16);

    const std::string path = sihd::sys::fs::combine(dir.path(), "put.bin");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content.data(), (std::streamsize)content.size());
    }

    auto resp = http::put(shard_url("localhost:3005/api/upload"), path);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), 200u);
    EXPECT_EQ(resp->content().cpp_str(), content);

    EXPECT_FALSE(http::put(shard_url("localhost:3005/api/upload"), "/no/such/file/here").has_value());
}

// a response cut short by max_response_size is still returned, truncated
TEST_F(TestRequest, test_max_response_size_overflow)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("big", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(std::string(8192, 'x'));
    });
    scope.start(shard_port(3005));

    RequestOptions options;
    options.max_response_size = 1024;
    auto response = http::get(shard_url("localhost:3005/api/big"), options);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::Ok);
    EXPECT_LE(response->content().size(), 1024u);

    auto unlimited_response = http::get(shard_url("localhost:3005/api/big"));
    ASSERT_TRUE(unlimited_response.has_value());
    EXPECT_EQ(unlimited_response->content().size(), 8192u);
}

TEST_F(TestRequest, test_request_options_parameters_and_progress)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point(
        "params",
        [](const HttpRequest & req, HttpResponse & resp) {
            resp.set_plain_content(req.query_param("q").value_or("missing"));
        },
        HttpRequest::Post);
    scope.start(shard_port(3005));

    RequestOptions options;
    options.parameters["q"] = "sihd";
    bool saw_upload_total = false;
    bool saw_download_total = false;
    options.progress = [&](const http::Progress & prog) {
        saw_upload_total = saw_upload_total || prog.upload_total == 4;
        saw_download_total = saw_download_total || prog.download_total > 0;
        return true;
    };

    auto response = http::post(shard_url("localhost:3005/api/params"), "body", options);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::Ok);
    EXPECT_EQ(response->content().cpp_str(), "sihd");
    EXPECT_TRUE(saw_upload_total);
    EXPECT_TRUE(saw_download_total);
}

} // namespace test
