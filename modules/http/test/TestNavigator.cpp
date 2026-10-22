#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
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
#include <sihd/http/request.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/time.hpp>

#include "http_test_helpers.hpp"

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::http;
using namespace sihd::util;
using namespace sihd::sys;

class TestNavigator: public ::testing::Test
{
    protected:
        TestNavigator() { LoggerManager::stream(); }
        virtual ~TestNavigator() { LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

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

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "navigator-ok");

    resp = navigator.post(shard_url("localhost:3001/api/echo"), "hello navigator");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "hello navigator");

    resp = navigator.put(shard_url("localhost:3001/api/echo_put"), "put data");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "put data");

    resp = navigator.del(shard_url("localhost:3001/api/del_ok"));
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

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.get(shard_url("localhost:3001/api/secure"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    navigator.set_basic_auth("admin", "secret");
    resp = navigator.get(shard_url("localhost:3001/api/secure"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "auth-ok");

    navigator.set_token_auth("my-secret-token-123");
    resp = navigator.get(shard_url("localhost:3001/api/secure"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "auth-ok");

    navigator.clear_auth();
    resp = navigator.get(shard_url("localhost:3001/api/secure"));
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

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.get(shard_url("localhost:3001/api/set_cookie"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = navigator.get(shard_url("localhost:3001/api/check_cookie"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "abc123");

    auto jar = navigator.cookies();
    EXPECT_FALSE(jar.empty());
    EXPECT_EQ(jar["session"], "abc123");
}

TEST_F(TestNavigator, test_navigator_redirect)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point(
        "hop1",
        redirect_to(HttpStatus::Found, shard_url("http://localhost:3001/api/dest")));
    scope.server._webservice->set_entry_point(
        "post_hop",
        redirect_to(HttpStatus::Found, shard_url("http://localhost:3001/api/dest")),
        HttpRequest::Post);
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.get(shard_url("localhost:3001/api/hop1"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_FALSE(resp->redirect_history().empty());
    EXPECT_EQ(resp->final_url(), shard_url("http://localhost:3001/api/dest"));
    EXPECT_FALSE(resp->was_method_downgraded());

    resp = navigator.post(shard_url("localhost:3001/api/post_hop"), "body");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "arrived");
    EXPECT_TRUE(resp->was_method_downgraded());
}

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

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_retry(3, 1);

    auto resp = navigator.get(shard_url("localhost:3001/api/flaky"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "ok");
    EXPECT_EQ(calls.load(), 3);
}

TEST_F(TestNavigator, test_navigator_retry_after_capped_by_max_wait)
{
    ServerScope scope;
    std::atomic<int> calls {0};

    scope.server._webservice->set_entry_point("limited", [&calls](const HttpRequest &, HttpResponse & resp) {
        if (calls.fetch_add(1) < 1)
        {
            resp.set_status(HttpStatus::TooManyRequests);
            resp.http_header().set_header("retry-after", "3600");
            resp.set_plain_content("slow down");
        }
        else
        {
            resp.set_plain_content("ok");
        }
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_retry(1, 1, 20);

    Stopwatch stopwatch;
    auto resp = navigator.get(shard_url("localhost:3001/api/limited"));
    const Duration elapsed = stopwatch.time();

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "ok");
    EXPECT_EQ(calls.load(), 2);
    EXPECT_LT(elapsed, Duration(time::sec(5)));
}

TEST_F(TestNavigator, test_navigator_max_response_size)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("big", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(std::string(4096, 'X'));
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_max_response_size(64);

    auto resp = navigator.get(shard_url("localhost:3001/api/big"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_LT(resp->content().size(), 4096u);
    EXPECT_TRUE(resp->truncated());

    navigator.set_max_response_size(0);
    auto whole = navigator.get(shard_url("localhost:3001/api/big"));
    ASSERT_TRUE(whole.has_value());
    EXPECT_EQ(whole->content().size(), 4096u);
    EXPECT_FALSE(whole->truncated());
}

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

    CsrfResult detected = csrf_extract_from_html(html);
    EXPECT_EQ(detected.field_name, "_csrf");
    EXPECT_EQ(detected.value, "tok3n-abc-XYZ");

    CsrfResult named = csrf_extract_from_html(html, std::string_view("_csrf"));
    EXPECT_EQ(named.value, "tok3n-abc-XYZ");

    CsrfResult unknown = csrf_extract_from_html(html, std::string_view("no_such_field"));
    EXPECT_TRUE(unknown.value.empty());
}

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

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.head(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().size(), 0u);
}

TEST_F(TestNavigator, test_navigator_redirect_policy_none)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point(
        "redir",
        redirect_to(HttpStatus::Found, shard_url("http://localhost:3001/api/dest")));
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_redirect_policy(RedirectPolicy::None);

    auto resp = navigator.get(shard_url("localhost:3001/api/redir"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Found);
    EXPECT_TRUE(resp->redirect_history().empty());
}

TEST_F(TestNavigator, test_navigator_meta_refresh)
{
    ServerScope scope;

    const std::string meta_page = shard_url(
        R"(<html><head><meta http-equiv="refresh" content="0; url=http://localhost:3001/api/dest"></head></html>)");
    scope.server._webservice->set_entry_point("meta_html", [&meta_page](const HttpRequest &, HttpResponse & resp) {
        ASSERT_TRUE(resp.set_content({meta_page.data(), meta_page.size()}).has_value());
        resp.set_content_type("text/html");
    });
    scope.server._webservice->set_entry_point(
        "meta_mixed_case",
        [&meta_page](const HttpRequest &, HttpResponse & resp) {
            ASSERT_TRUE(resp.set_content({meta_page.data(), meta_page.size()}).has_value());
            resp.set_content_type("Text/HTML; charset=UTF-8");
        });
    scope.server._webservice->set_entry_point("meta_plain", [&meta_page](const HttpRequest &, HttpResponse & resp) {
        ASSERT_TRUE(resp.set_plain_content(meta_page));
    });
    scope.server._webservice->set_entry_point("dest", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("arrived");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    auto html = navigator.get(shard_url("localhost:3001/api/meta_html"));
    ASSERT_TRUE(html.has_value());
    EXPECT_EQ(html->content().cpp_str(), "arrived");
    EXPECT_EQ(html->final_url(), shard_url("http://localhost:3001/api/dest"));

    auto mixed_case = navigator.get(shard_url("localhost:3001/api/meta_mixed_case"));
    ASSERT_TRUE(mixed_case.has_value());
    EXPECT_EQ(mixed_case->content().cpp_str(), "arrived");

    auto plain = navigator.get(shard_url("localhost:3001/api/meta_plain"));
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->content().cpp_str(), meta_page);
}

TEST_F(TestNavigator, test_navigator_custom_headers)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("echo", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_header("X-Custom", "test-value");

    auto resp = navigator.get(shard_url("localhost:3001/api/echo"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    navigator.remove_header("X-Custom");
    resp = navigator.get(shard_url("localhost:3001/api/echo"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    navigator.set_header("X-A", "1");
    navigator.set_header("X-B", "2");
    navigator.clear_headers();
    resp = navigator.get(shard_url("localhost:3001/api/echo"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
}

TEST_F(TestNavigator, test_navigator_user_agent_reaches_the_request)
{
    ServerScope scope;

    std::string seen_agent;
    scope.server._webservice->set_entry_point("echo", [&seen_agent](const HttpRequest & req, HttpResponse & resp) {
        seen_agent = std::string(req.http_header().find("user-agent"));
        resp.set_plain_content("ok");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_user_agent("sihd-navigator/1.0");

    auto resp = navigator.get(shard_url("localhost:3001/api/echo"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(seen_agent, "sihd-navigator/1.0");
}

TEST_F(TestNavigator, test_navigator_ws_send_receive)
{
    if constexpr (build::is_run_with_tsan)
        GTEST_SKIP_("has weird interaction with lws and tsan");

    SimpleHttpServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(shard_port(3003));

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ws-navigator-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));

    Navigator navigator;
    allow_local_targets(navigator);

    std::optional<std::string> ws_reply;
    Waitable reply_waitable;

    navigator.on_ws_text = [&](std::string_view msg) {
        auto reply_lock = reply_waitable.guard();
        ws_reply = std::string(msg);
        reply_waitable.notify_all();
    };

    ASSERT_TRUE(navigator.ws_connect(shard_url("ws://localhost:3003"), "proto-two"));
    ASSERT_TRUE(navigator.ws_send("hello from navigator"));

    reply_waitable.wait_for(Duration(time::sec(2)), [&] { return ws_reply.has_value(); });
    ASSERT_TRUE(ws_reply.has_value());
    EXPECT_EQ(*ws_reply, "hello world");

    navigator.ws_close();

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

    Navigator navigator;
    allow_local_targets(navigator);

    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine({tmp_dir.path(), "download.txt"});
    auto resp = navigator.download(shard_url("localhost:3001/api/file"), path);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().size(), 0u);
    EXPECT_EQ(fs::read_all(path), payload);
}

TEST_F(TestNavigator, test_navigator_put_file)
{
    ServerScope scope;
    std::string received;

    scope.server._webservice->set_entry_point(
        "upload",
        [&](const HttpRequest & req, HttpResponse & resp) {
            received = req.content().cpp_str();
            resp.set_plain_content("ok");
        },
        HttpRequest::Put);

    scope.start();

    // larger than one curl read buffer: the file must stream through the read callback
    std::string content(256 * 1024, 'x');
    for (size_t i = 0; i < content.size(); i += 16)
        content.replace(i, 16, "0123456789abcdef");

    sihd::sys::TmpDir dir;
    ASSERT_TRUE(dir);
    const std::string path = sihd::sys::fs::combine(dir.path(), "upload.bin");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content.data(), (std::streamsize)content.size());
    }

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.put_file(shard_url("localhost:3001/api/upload"), path);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(received, content);

    EXPECT_FALSE(navigator.put_file(shard_url("localhost:3001/api/upload"), "/no/such/file/here").has_value());
}

// one navigator: the transfer handle is reused, connections stay alive
TEST_F(TestNavigator, test_navigator_connection_reuse)
{
    ServerScope scope;
    int get_count = 0;

    scope.server._webservice->set_entry_point("counter", [&get_count](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("hit-" + std::to_string(++get_count));
    });
    scope.server._webservice->set_entry_point(
        "counter",
        [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
        HttpRequest::Post);

    scope.start(shard_port(3005));

    Navigator navigator;
    allow_local_targets(navigator);

    for (int i = 1; i <= 3; ++i)
    {
        auto response = navigator.get(shard_url("localhost:3005/api/counter"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "hit-" + std::to_string(i));
        EXPECT_EQ(navigator.new_connection_count(), i == 1 ? 1 : 0);
    }

    {
        auto response = navigator.post(shard_url("localhost:3005/api/counter"), "posted");
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "posted");
        EXPECT_EQ(navigator.new_connection_count(), 0);
    }

    {
        auto response = navigator.get(shard_url("localhost:3005/api/counter"));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->content().cpp_str(), "hit-4");
        EXPECT_EQ(navigator.new_connection_count(), 0);
    }
    EXPECT_EQ(get_count, 4);
}

TEST_F(TestNavigator, test_navigator_interceptor_observe)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("intercepted");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    int before_calls = 0;
    int after_calls = 0;
    std::string seen_method;
    int seen_status = 0;

    navigator.on_before_request = [&](RequestInfo & info) {
        before_calls++;
        seen_method = info.method;
        return true;
    };
    navigator.on_after_response = [&](const NavigatorResponse & resp) {
        after_calls++;
        seen_status = resp.status();
    };

    auto resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(before_calls, 1);
    EXPECT_EQ(after_calls, 1);
    EXPECT_EQ(seen_method, "GET");
    EXPECT_EQ(seen_status, 200);
}

TEST_F(TestNavigator, test_navigator_interceptor_rewrite)
{
    ServerScope scope;

    std::string seen_content_type;

    scope.server._webservice->set_entry_point(
        "echo",
        [&](const HttpRequest & req, HttpResponse & resp) {
            seen_content_type = std::string(req.http_header().content_type().value_or(""));
            resp.set_plain_content(req.content().cpp_str());
        },
        HttpRequest::Post);

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    navigator.on_before_request = [](RequestInfo & info) {
        info.url = shard_url("localhost:3001/api/echo");
        info.method = "POST";
        info.headers["content-type"] = "application/sihd";
        return true;
    };

    auto resp = navigator.get(shard_url("localhost:3001/api/unused"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(seen_content_type, "application/sihd");
}

TEST_F(TestNavigator, test_navigator_multipart)
{
    ServerScope scope;

    std::string seen_username;
    std::string seen_bio;

    scope.server._webservice->set_entry_point(
        "upload",
        [&](const HttpRequest & req, HttpResponse & resp) {
            if (const Multipart *multipart = req.multipart(); multipart != nullptr)
            {
                seen_username = multipart->value("username").value_or("");
                seen_bio = multipart->value("bio").value_or("");
            }
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    Multipart form;
    form.add_field("username", "testuser");
    form.add_field("bio", "hello world");

    auto resp = navigator.post_multipart(shard_url("localhost:3001/api/upload"), form);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    EXPECT_EQ(seen_username, "testuser");
    EXPECT_EQ(seen_bio, "hello world");
}

TEST_F(TestNavigator, test_navigator_multipart_file)
{
    ServerScope scope;

    std::string seen_field;
    std::string seen_filename;
    std::string seen_content_type;
    std::string seen_data;

    scope.server._webservice->set_entry_point(
        "upload",
        [&](const HttpRequest & req, HttpResponse & resp) {
            if (const Multipart *multipart = req.multipart(); multipart != nullptr)
            {
                seen_field = multipart->value("description").value_or("");
                if (const Multipart::Part *file = multipart->file("document"); file != nullptr)
                {
                    seen_filename = file->filename;
                    seen_content_type = file->content_type;
                    seen_data = file->data;
                }
            }
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    sihd::sys::TmpDir dir;
    ASSERT_TRUE(dir);
    const std::string path = sihd::sys::fs::combine(dir.path(), "multipart.txt");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "file payload";
    }

    Multipart form;
    form.add_field("description", "a file");
    form.add_file("document", path, "document.txt", "text/plain");

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.post_multipart(shard_url("localhost:3001/api/upload"), form);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(seen_field, "a file");
    EXPECT_EQ(seen_filename, "document.txt");
    EXPECT_EQ(seen_content_type, "text/plain");
    EXPECT_EQ(seen_data, "file payload");
}

TEST_F(TestNavigator, test_navigator_multipart_malformed)
{
    ServerScope scope;

    int calls = 0;

    scope.server._webservice->set_entry_point(
        "upload",
        [&](const HttpRequest &, HttpResponse & resp) {
            calls++;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    RequestOptions options;
    options.headers["content-type"] = "multipart/form-data; boundary=boundary";

    auto resp = sihd::http::post(shard_url("localhost:3001/api/upload"), "not a multipart body", options);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::BadRequest);
    EXPECT_EQ(calls, 0);
}

TEST_F(TestNavigator, test_navigator_interceptor_cancel)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("should-not-reach");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    navigator.on_before_request = [](RequestInfo &) {
        return false;
    };

    auto resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().message, "request cancelled by the on_before_request interceptor");

    navigator.on_before_request = nullptr;
    resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());
}

TEST_F(TestNavigator, test_navigator_error_message)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("navigator-ok");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    auto resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());

    resp = navigator.get("localhost:19999/api/hello");
    ASSERT_FALSE(resp.has_value());
    EXPECT_FALSE(resp.error().message.empty());

    resp = navigator.get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());

    navigator.set_ssrf_guard(true);
    resp = navigator.get(shard_url("http://127.0.0.1:3001/api/hello"));
    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().message,
              shard_url("SSRF guard blocked a request to a private host: http://127.0.0.1:3001/api/hello"));

    // curl takes a URL without scheme, the guard must see the host all the same
    resp = navigator.get(shard_url("127.0.0.1:3001/api/hello"));
    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().message,
              shard_url("SSRF guard blocked a request to a private host: 127.0.0.1:3001/api/hello"));
}

TEST_F(TestNavigator, test_navigator_ws_proxy_auth)
{
    if constexpr (build::is_run_with_tsan)
        GTEST_SKIP_("has weird interaction with lws and tsan");

    SimpleConnectProxy proxy("testuser", "testpass");
    ASSERT_TRUE(proxy.start(shard_port(3099)));

    SimpleHttpServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(shard_port(3013));

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ws-proxy-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));

    const auto run_ws = [](const std::string & user, const std::string & pass) {
        Navigator navigator;
        navigator.set_ssrf_guard(false);
        navigator.set_proxy(shard_url("localhost:3099"));
        navigator.set_proxy_auth(user, pass);

        std::optional<std::string> reply;
        Waitable waitable;
        navigator.on_ws_text = [&](std::string_view msg) {
            auto reply_lock = waitable.guard();
            reply = std::string(msg);
            waitable.notify_all();
        };
        const bool connected = navigator.ws_connect(shard_url("ws://localhost:3013"), "proto-two").has_value();
        if (connected)
        {
            navigator.ws_send("hello from navigator");
            waitable.wait_for(Duration(time::sec(2)), [&] { return reply.has_value(); });
            navigator.ws_close();
        }
        return std::make_pair(connected, reply);
    };

    auto [ok_connected, ok_reply] = run_ws("testuser", "testpass");
    EXPECT_TRUE(ok_connected);
    ASSERT_TRUE(ok_reply.has_value());
    EXPECT_EQ(*ok_reply, "hello world");

    // wrong credentials: proxy returns 407, connection must fail (proves proxy is actually used)
    auto [bad_connected, bad_reply] = run_ws("wrong", "creds");
    EXPECT_FALSE(bad_connected);
    EXPECT_FALSE(bad_reply.has_value());

    EXPECT_GE(proxy.n_200, 1);
    EXPECT_GE(proxy.n_407, 1);

    server.set_service_wait_stop(true);
    server.stop();
    proxy.stop();
}

} // namespace test
