#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/http/navigator/LinkExtractor.hpp>
#include <sihd/http/navigator/RobotsTxt.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

#include "http_test_helpers.hpp"

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;
using namespace sihd::util;

class TestNavigator: public ::testing::Test
{
    protected:
        TestNavigator() { sihd::util::LoggerManager::stream(); }
        virtual ~TestNavigator() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

// --- Tests ---

TEST_F(TestNavigator, test_navigator_basic)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("navigator-ok");
    });
    scope.server._webservice->set_entry_point(
        "echo",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Post);
    scope.server._webservice->set_entry_point(
        "echo_put",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Put);
    scope.server._webservice->set_entry_point(
        "del_ok",
        [](const HttpRequest &, HttpResponse & resp) {
            resp.set_status(HttpStatus::Ok);
            resp.set_plain_content("deleted");
        },
        HttpRequest::Delete);

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    auto resp = nav.get("localhost:3001/api/hello");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "navigator-ok");

    resp = nav.post("localhost:3001/api/echo", "hello navigator");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "hello navigator");

    resp = nav.put("localhost:3001/api/echo_put", "put data");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "put data");

    resp = nav.del("localhost:3001/api/del_ok");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "deleted");
}

TEST_F(TestNavigator, test_navigator_auth)
{
    ServerScope scope;
    TestAuth auth;

    scope.server.set_authenticator(&auth);
    scope.server._webservice->set_entry_point("secure", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("auth-ok");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    auto resp = nav.get("localhost:3001/api/secure");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    nav.set_basic_auth("admin", "secret");
    resp = nav.get("localhost:3001/api/secure");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "auth-ok");

    nav.set_token_auth("my-secret-token-123");
    resp = nav.get("localhost:3001/api/secure");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "auth-ok");

    nav.clear_auth();
    resp = nav.get("localhost:3001/api/secure");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);
}

TEST_F(TestNavigator, test_navigator_cookies)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("set_cookie", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_cookie("session", "abc123");
        resp.set_plain_content("cookie-set");
    });
    scope.server._webservice->set_entry_point("check_cookie", [](const HttpRequest & req, HttpResponse & resp) {
        resp.set_plain_content(req.cookie("session").value_or(""));
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    auto resp = nav.get("localhost:3001/api/set_cookie");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = nav.get("localhost:3001/api/check_cookie");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "abc123");

    auto jar = nav.cookies();
    EXPECT_FALSE(jar.empty());
    EXPECT_EQ(jar["session"], "abc123");
}

// Manual redirect loop: verifies redirect_history, final_url, and POST→GET downgrade
TEST_F(TestNavigator, test_navigator_redirect)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hop1", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_status(HttpStatus::Found);
        resp.http_header().set_header("location", "http://localhost:3001/api/dest");
    });
    scope.server._webservice->set_entry_point(
        "post_hop",
        [](const HttpRequest &, HttpResponse & resp) {
            resp.set_status(HttpStatus::Found);
            resp.http_header().set_header("location", "http://localhost:3001/api/dest");
        },
        HttpRequest::Post);
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    // GET → 302 → GET
    auto resp = nav.get("localhost:3001/api/hop1");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_FALSE(resp->redirect_history().empty());
    EXPECT_NE(resp->final_url().find("/api/dest"), std::string::npos);
    EXPECT_FALSE(resp->was_method_downgraded());

    // POST → 302 → GET (method downgrade)
    resp = nav.post("localhost:3001/api/post_hop", "body");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_TRUE(resp->was_method_downgraded());
}

// Retry on transient 503: server fails twice then succeeds
TEST_F(TestNavigator, test_navigator_retry)
{
    ServerScope scope;
    std::atomic<int> calls {0};

    scope.server._webservice->set_entry_point("flaky", [&calls](const HttpRequest &, HttpResponse & resp) {
        if (calls.fetch_add(1) < 2)
        {
            resp.set_status(HttpStatus::ServiceUnavailable);
            resp.set_plain_content("not ready");
        }
        else
        {
            resp.set_plain_content("ok");
        }
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();
    nav.set_retry(3, 1); // up to 3 attempts, 1 ms initial backoff

    auto resp = nav.get("localhost:3001/api/flaky");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "ok");
    EXPECT_EQ(calls.load(), 3);
}

// Max response size cap
TEST_F(TestNavigator, test_navigator_max_response_size)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("big", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(std::string(4096, 'X'));
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();
    nav.set_max_response_size(64);

    auto resp = nav.get("localhost:3001/api/big");
    ASSERT_TRUE(resp.has_value());
    EXPECT_LT(resp->content().size(), 4096u);
}

// CSRF token extraction from HTML without any network
TEST_F(TestNavigator, test_csrf_extractor)
{
    const std::string html = R"(
<html><body>
<form method="post" action="/login">
  <input type="text" name="username" />
  <input type="hidden" name="_csrf" value="tok3n-abc-XYZ" />
  <button>Login</button>
</form>
</body></html>
)";

    CsrfResult r = csrf_extract_from_html(html);
    EXPECT_EQ(r.field_name, "_csrf");
    EXPECT_EQ(r.value, "tok3n-abc-XYZ");

    CsrfResult r2 = csrf_extract_from_html(html, std::string_view("_csrf"));
    EXPECT_EQ(r2.value, "tok3n-abc-XYZ");

    CsrfResult r3 = csrf_extract_from_html(html, std::string_view("no_such_field"));
    EXPECT_TRUE(r3.value.empty());
}

// robots.txt parsing (RFC 9309) without any network
TEST_F(TestNavigator, test_robots_txt)
{
    const std::string_view content = R"(
User-agent: *
Disallow: /private/
Disallow: /admin/
Allow: /admin/public/
Crawl-delay: 2

User-agent: Googlebot
Allow: /
)";

    RobotsTxt robots;
    robots.parse(content);

    EXPECT_TRUE(robots.is_allowed("anybot", "/index.html"));
    EXPECT_FALSE(robots.is_allowed("anybot", "/private/secret"));
    EXPECT_FALSE(robots.is_allowed("anybot", "/admin/dashboard"));
    EXPECT_TRUE(robots.is_allowed("anybot", "/admin/public/page"));

    auto delay = robots.crawl_delay("anybot");
    ASSERT_TRUE(delay.has_value());
    EXPECT_EQ(*delay, 2L);

    EXPECT_TRUE(robots.is_allowed("Googlebot", "/anything"));
}

TEST_F(TestNavigator, test_link_extractor)
{
    const std::string html = R"(
<html><body>
<a href="/about" rel="nofollow">About Us</a>
<a href="https://example.com/page">External</a>
<img src="/img/logo.png" alt="Logo" />
<link href="/css/style.css" rel="stylesheet" />
<script src="/js/app.js"></script>
</body></html>
)";

    auto links = extract_links(html, "https://mysite.com");

    ASSERT_EQ(links.size(), 5u);

    EXPECT_EQ(links[0].tag, "a");
    EXPECT_EQ(links[0].url, "https://mysite.com/about");
    EXPECT_EQ(links[0].rel, "nofollow");

    EXPECT_EQ(links[1].tag, "a");
    EXPECT_EQ(links[1].url, "https://example.com/page");

    EXPECT_EQ(links[2].tag, "img");
    EXPECT_EQ(links[2].url, "https://mysite.com/img/logo.png");
    EXPECT_EQ(links[2].text, "Logo");

    EXPECT_EQ(links[3].tag, "link");
    EXPECT_EQ(links[3].url, "https://mysite.com/css/style.css");
    EXPECT_EQ(links[3].rel, "stylesheet");

    EXPECT_EQ(links[4].tag, "script");
    EXPECT_EQ(links[4].url, "https://mysite.com/js/app.js");

    auto no_base = extract_links(html);
    EXPECT_FALSE(no_base.empty());
    EXPECT_EQ(no_base[0].url, "/about");
}

TEST_F(TestNavigator, test_navigator_head)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("should-not-appear-in-head");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    auto resp = nav.head("localhost:3001/api/hello");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().size(), 0u);
}

TEST_F(TestNavigator, test_navigator_redirect_policy_none)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("redir", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_status(HttpStatus::Found);
        resp.http_header().set_header("location", "http://localhost:3001/api/dest");
    });
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();
    nav.set_redirect_policy(RedirectPolicy::None);

    auto resp = nav.get("localhost:3001/api/redir");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Found);
    EXPECT_TRUE(resp->redirect_history().empty());
}

TEST_F(TestNavigator, test_navigator_custom_headers)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("echo", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();
    nav.set_header("X-Custom", "test-value");

    auto resp = nav.get("localhost:3001/api/echo");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    nav.remove_header("X-Custom");
    resp = nav.get("localhost:3001/api/echo");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    nav.set_header("X-A", "1");
    nav.set_header("X-B", "2");
    nav.clear_headers();
    resp = nav.get("localhost:3001/api/echo");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
}

TEST_F(TestNavigator, test_navigator_ws_send_receive)
{
    if constexpr (sihd::util::build::is_run_with_tsan)
        GTEST_SKIP_("has weird interaction with lws and tsan");

    SimpleWsServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(3003);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ws-nav-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));

    Navigator nav;
    nav.clear_proxy();
    std::optional<std::string> ws_reply;
    std::mutex reply_mutex;
    std::condition_variable reply_cv;

    nav.on_ws_text = [&](std::string_view msg) {
        std::lock_guard lock(reply_mutex);
        ws_reply = std::string(msg);
        reply_cv.notify_all();
    };

    ASSERT_TRUE(nav.ws_connect("ws://localhost:3003", "proto-two"));
    ASSERT_TRUE(nav.ws_send("hello from navigator"));

    {
        std::unique_lock lock(reply_mutex);
        reply_cv.wait_for(lock, std::chrono::seconds(2), [&] { return ws_reply.has_value(); });
    }
    ASSERT_TRUE(ws_reply.has_value());
    EXPECT_EQ(*ws_reply, "hello world");

    nav.ws_close();

    server.set_service_wait_stop(true);
    server.stop();

    EXPECT_EQ(server._nopen, 1);
    EXPECT_EQ(server._nread, 1);
    EXPECT_EQ(server._nwrite, 1);
    EXPECT_GE(server._nclosed, 1);
}

TEST_F(TestNavigator, test_navigator_download)
{
    ServerScope scope;

    const std::string payload = "downloaded-content-1234";

    scope.server._webservice->set_entry_point("file", [&](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(payload);
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    const std::string path = "/tmp/sihd_test_download.txt";
    auto resp = nav.download("localhost:3001/api/file", path);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().size(), 0u);

    std::ifstream ifs(path);
    ASSERT_TRUE(ifs.is_open());
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, payload);

    std::remove(path.c_str());
}

TEST_F(TestNavigator, test_navigator_interceptor_observe)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("intercepted");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    int before_calls = 0;
    int after_calls = 0;
    std::string seen_method;
    int seen_status = 0;

    nav.on_before_request = [&](RequestInfo & info) {
        before_calls++;
        seen_method = info.method;
        return true;
    };
    nav.on_after_response = [&](const NavigatorResponse & resp) {
        after_calls++;
        seen_status = resp.status();
    };

    auto resp = nav.get("localhost:3001/api/hello");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(before_calls, 1);
    EXPECT_EQ(after_calls, 1);
    EXPECT_EQ(seen_method, "GET");
    EXPECT_EQ(seen_status, 200);
}

TEST_F(TestNavigator, test_navigator_multipart)
{
    ServerScope scope;

    std::string received_body;

    scope.server._webservice->set_entry_point(
        "upload",
        [&](const HttpRequest & req, HttpResponse & resp) {
            received_body = req.content().cpp_str();
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    std::vector<MultipartField> fields;
    fields.push_back({.name = "username", .value = "testuser", .is_file = false, .content_type = "", .filename = ""});
    fields.push_back({.name = "bio", .value = "hello world", .is_file = false, .content_type = "", .filename = ""});

    auto resp = nav.post_multipart("localhost:3001/api/upload", fields);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    EXPECT_NE(received_body.find("username"), std::string::npos);
    EXPECT_NE(received_body.find("testuser"), std::string::npos);
    EXPECT_NE(received_body.find("bio"), std::string::npos);
    EXPECT_NE(received_body.find("hello world"), std::string::npos);
}

TEST_F(TestNavigator, test_navigator_interceptor_cancel)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("should-not-reach");
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    nav.on_before_request = [](RequestInfo &) {
        return false;
    };

    auto resp = nav.get("localhost:3001/api/hello");
    EXPECT_FALSE(resp.has_value());
}

} // namespace test
