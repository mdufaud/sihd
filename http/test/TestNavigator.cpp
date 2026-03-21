#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <string_view>

#include "http_test_helpers.hpp"

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

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

    scope.server._webservice->set_entry_point("check_cookie",
                                              [](const HttpRequest & req, HttpResponse & resp) {
                                                  resp.set_plain_content(req.cookie("session").value_or(""));
                                              });

    scope.start();

    Navigator nav;

    auto resp = nav.get("localhost:3001/api/set_cookie");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = nav.get("localhost:3001/api/check_cookie");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "abc123");

    auto cookies = nav.cookies();
    EXPECT_FALSE(cookies.empty());
    EXPECT_EQ(cookies["session"], "abc123");
}

TEST_F(TestNavigator, test_navigator_persistent_headers)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("headers", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });

    scope.start();

    Navigator nav;
    nav.set_header("X-Custom", "my-value");

    auto resp = nav.get("localhost:3001/api/headers");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = nav.get("localhost:3001/api/headers");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    nav.remove_header("X-Custom");
    resp = nav.get("localhost:3001/api/headers");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
}

TEST_F(TestNavigator, test_navigator_ws_send_receive)
{
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

} // namespace test
