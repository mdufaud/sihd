#include <gtest/gtest.h>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

class TestNavigatorRedirects: public ::testing::Test
{
    protected:
        TestNavigatorRedirects() { LoggerManager::stream(); }
        virtual ~TestNavigatorRedirects() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNavigatorRedirects, test_navigator_relative_redirect)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("absolute_path", redirect_to(HttpStatus::Found, "/api/dest"));
    scope.server._webservice->set_entry_point("sibling", redirect_to(HttpStatus::Found, "dest"));
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_redirect_policy(RedirectPolicy::SameHost);

    auto resp = navigator.get(shard_url("localhost:3001/api/absolute_path"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_EQ(resp->final_url(), shard_url("http://localhost:3001/api/dest"));

    resp = navigator.get(shard_url("localhost:3001/api/sibling"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_EQ(resp->final_url(), shard_url("http://localhost:3001/api/dest"));
}

TEST_F(TestNavigatorRedirects, test_navigator_relative_meta_refresh)
{
    ServerScope scope;

    const std::string meta_page = R"(<html><head><meta http-equiv="refresh" content="0; url=dest"></head></html>)";
    scope.server._webservice->set_entry_point("meta_relative", [&meta_page](const HttpRequest &, HttpResponse & resp) {
        EXPECT_TRUE(resp.set_content({meta_page.data(), meta_page.size()}).has_value());
        resp.set_content_type("text/html");
    });
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_redirect_policy(RedirectPolicy::SameHost);

    auto resp = navigator.get(shard_url("localhost:3001/api/meta_relative"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_EQ(resp->final_url(), shard_url("http://localhost:3001/api/dest"));
}

TEST_F(TestNavigatorRedirects, test_navigator_meta_refresh_url_key_is_a_whole_token)
{
    ServerScope scope;

    const std::string meta_page =
        R"(<html><head><meta http-equiv="refresh" content="5;curl=http://evil.example/;url=/api/next"></head></html>)";
    scope.server._webservice->set_entry_point("meta_curl", [&meta_page](const HttpRequest &, HttpResponse & resp) {
        EXPECT_TRUE(resp.set_content({meta_page.data(), meta_page.size()}).has_value());
        resp.set_content_type("text/html");
    });
    scope.server._webservice->set_entry_point("next", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_redirect_policy(RedirectPolicy::SameHost);

    auto resp = navigator.get(shard_url("localhost:3001/api/meta_curl"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_EQ(resp->final_url(), shard_url("http://localhost:3001/api/next"));
}

TEST_F(TestNavigatorRedirects, test_navigator_same_domain_policy_does_not_group_ip_addresses)
{
    ServerScope scope;

    scope.server.set_interface("");
    scope.server._webservice->set_entry_point(
        "hop",
        redirect_to(HttpStatus::Found, shard_url("http://127.9.5.1:3001/api/dest")));
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_redirect_policy(RedirectPolicy::SameDomain);

    auto resp = navigator.get(shard_url("127.0.5.1:3001/api/hop"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Found);
    EXPECT_TRUE(resp->redirect_history().empty());
}

TEST_F(TestNavigatorRedirects, test_navigator_meta_refresh_hostile_html)
{
    ServerScope scope;

    std::string hostile = "<html><meta ";
    for (int i = 0; i < 3000; ++i)
        hostile += "http-equiv=refresh ";

    scope.server._webservice->set_entry_point("hostile", [&hostile](const HttpRequest &, HttpResponse & resp) {
        EXPECT_TRUE(resp.set_content({hostile.data(), hostile.size()}).has_value());
        resp.set_content_type("text/html");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    Stopwatch stopwatch;
    auto resp = navigator.get(shard_url("localhost:3001/api/hostile"));
    const Duration elapsed = stopwatch.time();

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().size(), hostile.size());
    if constexpr (build::is_run_with_sanitizer == false)
    {
        EXPECT_LT(elapsed, Duration(time::sec(1)));
    }
}

TEST_F(TestNavigatorRedirects, test_navigator_redirect_to_non_http_scheme_is_not_followed)
{
    ServerScope scope;

    const std::string
        meta_page = R"(<html><head><meta http-equiv="refresh" content="0; url=file:///etc/passwd"></head></html>)";
    scope.server._webservice->set_entry_point("to_file", redirect_to(HttpStatus::Found, "file:///etc/passwd"));
    scope.server._webservice->set_entry_point("to_gopher",
                                              redirect_to(HttpStatus::Found, "gopher://127.0.0.1:6379/_PING"));
    scope.server._webservice->set_entry_point("meta_file", [&meta_page](const HttpRequest &, HttpResponse & resp) {
        EXPECT_TRUE(resp.set_content({meta_page.data(), meta_page.size()}).has_value());
        resp.set_content_type("text/html");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    for (const std::string & path :
         {shard_url("localhost:3001/api/to_file"), shard_url("localhost:3001/api/to_gopher")})
    {
        auto resp = navigator.get(path);
        ASSERT_TRUE(resp.has_value()) << path;
        EXPECT_EQ(resp->status(), HttpStatus::Found);
        EXPECT_TRUE(resp->redirect_history().empty());
        EXPECT_EQ(resp->final_url(), path);
        EXPECT_FALSE(resp->redirect_refusal().empty());
    }

    auto meta = navigator.get(shard_url("localhost:3001/api/meta_file"));
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->content().cpp_str(), meta_page);
    EXPECT_EQ(meta->final_url(), shard_url("localhost:3001/api/meta_file"));
}

TEST_F(TestNavigatorRedirects, test_navigator_cross_origin_redirect_drops_auth)
{
    ServerScope scope;
    TestAuth auth;

    scope.server.set_authenticator(&auth);
    scope.server._webservice->set_entry_point("secure", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("auth-ok");
    });
    scope.server._webservice->set_entry_point(
        "to_secure_same",
        redirect_to(HttpStatus::Found, shard_url("http://localhost:3001/api/secure")));
    scope.server._webservice->set_entry_point(
        "to_secure_other_host",
        redirect_to(HttpStatus::Found, shard_url("http://127.0.0.1:3001/api/secure")));

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_basic_auth("admin", "secret");

    auto resp = navigator.get(shard_url("localhost:3001/api/to_secure_same"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "auth-ok");

    resp = navigator.get(shard_url("localhost:3001/api/to_secure_other_host"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    navigator.set_token_auth("my-secret-token-123");
    resp = navigator.get(shard_url("localhost:3001/api/to_secure_other_host"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);
}

TEST_F(TestNavigatorRedirects, test_navigator_cross_origin_redirect_drops_custom_headers)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("echo_headers", [](const HttpRequest & req, HttpResponse & resp) {
        resp.set_plain_content(
            fmt::format("cookie=[{}] accept=[{}]", req.http_header().find("cookie"), req.http_header().find("accept")));
    });
    scope.server._webservice->set_entry_point(
        "to_echo_same",
        redirect_to(HttpStatus::Found, shard_url("http://localhost:3001/api/echo_headers")));
    scope.server._webservice->set_entry_point(
        "to_echo_other_host",
        redirect_to(HttpStatus::Found, shard_url("http://127.0.0.1:3001/api/echo_headers")));

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_header("Cookie", "session=s3cr3t");
    navigator.set_header("Accept", "text/plain");

    auto resp = navigator.get(shard_url("localhost:3001/api/to_echo_same"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "cookie=[session=s3cr3t] accept=[text/plain]");

    resp = navigator.get(shard_url("localhost:3001/api/to_echo_other_host"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "cookie=[] accept=[*/*]");

    navigator.set_cross_origin_headers({"ACCEPT"});
    resp = navigator.get(shard_url("localhost:3001/api/to_echo_other_host"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "cookie=[] accept=[text/plain]");
}

TEST_F(TestNavigatorRedirects, test_navigator_cross_origin_redirect_refuses_payload)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point(
        "post_to_other_host",
        redirect_to(HttpStatus::TemporaryRedirect, shard_url("http://127.0.0.1:3001/api/sink")),
        HttpRequest::Post);
    scope.server._webservice->set_entry_point(
        "post_to_same_host",
        redirect_to(HttpStatus::TemporaryRedirect, shard_url("http://localhost:3001/api/sink")),
        HttpRequest::Post);
    scope.server._webservice->set_entry_point(
        "sink",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Post);

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.post(shard_url("localhost:3001/api/post_to_same_host"), "password=hunter2");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "password=hunter2");

    resp = navigator.post(shard_url("localhost:3001/api/post_to_other_host"), "password=hunter2");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::TemporaryRedirect);
    EXPECT_TRUE(resp->redirect_history().empty());
    EXPECT_EQ(resp->redirect_refusal(),
              shard_url("cross-origin redirect refused for a request with a payload: http://127.0.0.1:3001/api/sink"));
}

} // namespace test
