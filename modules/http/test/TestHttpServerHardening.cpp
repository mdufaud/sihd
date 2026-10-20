#include <gtest/gtest.h>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/request.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;
using namespace sihd::sys;

class TestHttpServerHardening: public ::testing::Test
{
    protected:
        TestHttpServerHardening() { LoggerManager::stream(); }
        virtual ~TestHttpServerHardening() { LoggerManager::clear_loggers(); }
};

TEST_F(TestHttpServerHardening, test_cors_is_off_by_default)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("hello");
    });
    scope.start();

    auto resp = sihd::http::get(shard_url("localhost:3001/api/hello"));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->http_header().find("access-control-allow-origin"), "");
}

TEST_F(TestHttpServerHardening, test_half_configured_tls_refuses_to_start)
{
    TmpDir dir;
    const std::string cert_path = fs::combine(dir.path(), "cert.pem");
    ASSERT_TRUE(fs::write(cert_path, "cert").has_value());

    FeatureHttpServer server;
    server.set_port(shard_port(3002));
    ASSERT_TRUE(server.set_ssl_cert_path(cert_path));

    EXPECT_FALSE(server.start());
}

TEST_F(TestHttpServerHardening, test_static_serving_stays_in_its_roots)
{
    TmpDir dir;
    const std::string root = fs::combine(dir.path(), "root");
    ASSERT_TRUE(fs::make_directory(root).has_value());
    ASSERT_TRUE(fs::write(fs::combine(root, "plain.txt"), "plain").has_value());
    ASSERT_TRUE(fs::write(fs::combine(dir.path(), "secret.txt"), "secret").has_value());

    if constexpr (build::is_windows == false)
    {
        ASSERT_TRUE(fs::write(fs::combine(root, "odd\\name.txt"), "odd").has_value());
        ASSERT_TRUE(
            fs::make_file_link(fs::combine(dir.path(), "secret.txt"), fs::combine(root, "link.txt")).has_value());
    }

    FeatureHttpServer served_from_root;
    ASSERT_TRUE(served_from_root.set_root_dir(root));
    FeatureHttpServer served_from_resources;
    ASSERT_TRUE(served_from_resources.add_resource_path(root));

    for (FeatureHttpServer *server : {&served_from_root, &served_from_resources})
    {
        EXPECT_TRUE(server->get_resource_path("/plain.txt").has_value());
        EXPECT_FALSE(server->get_resource_path("/../secret.txt").has_value());
        EXPECT_FALSE(server->get_resource_path("/..\\secret.txt").has_value());
        EXPECT_FALSE(server->get_resource_path("/link.txt").has_value());
        EXPECT_EQ(server->get_resource_path("/odd\\name.txt").has_value(), build::is_windows == false);
    }
}

TEST_F(TestHttpServerHardening, test_routing_path_is_decoded_once)
{
    ServerScope scope;
    TestFilter filter;
    filter.blocked_uri = "/api/secret";

    scope.server.set_http_filter(&filter);
    scope.server._webservice->set_entry_point("files/{path...}", [](const HttpRequest & req, HttpResponse & resp) {
        resp.set_plain_content(std::string(req.path_param("path").value_or("")));
    });
    scope.server._webservice->set_entry_point("secret", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("hidden");
    });

    scope.start();

    auto encoded = sihd::http::get(shard_url("localhost:3001/api/files/%252e%252e%252fetc%252fpasswd"));
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(encoded->status(), HttpStatus::Ok);
    EXPECT_EQ(encoded->content().cpp_str(), "%2e%2e%2fetc%2fpasswd");

    auto single = sihd::http::get(shard_url("localhost:3001/api/files/a%20b"));
    ASSERT_TRUE(single.has_value());
    EXPECT_EQ(single->content().cpp_str(), "a b");

    auto double_encoded = sihd::http::get(shard_url("localhost:3001/api/%2573ecret"));
    ASSERT_TRUE(double_encoded.has_value());
    EXPECT_EQ(double_encoded->status(), HttpStatus::NotFound);
}

TEST_F(TestHttpServerHardening, test_announced_body_is_not_allocated_up_front)
{
    if constexpr (build::is_run_with_sanitizer)
        GTEST_SKIP_("sanitizer allocators make the RSS delta meaningless");

    ServerScope scope;

    scope.server._webservice->set_entry_point(
        "sink",
        [](const HttpRequest &, HttpResponse & resp) { resp.set_plain_content("ok"); },
        HttpRequest::Post);
    scope.server._webservice->set_entry_point("ping", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("pong");
    });
    scope.start();

    constexpr size_t client_count = 12;
    constexpr size_t announced_body_size = 16 * 1024 * 1024;
    constexpr ssize_t eager_allocation = client_count * announced_body_size;
    // one sixth of what eager allocation would take: well above the per connection bookkeeping
    constexpr ssize_t allowed_growth = eager_allocation / 6;
    const std::string announcement = fmt::format(
        "POST /api/sink HTTP/1.1\r\nHost: localhost\r\nContent-Length: {}\r\n\r\n",
        announced_body_size);
    const ssize_t rss_before = sihd::sys::os::current_rss();

    std::vector<std::unique_ptr<RawHttpClient>> silent_clients;
    for (size_t i = 0; i < client_count; ++i)
    {
        auto & client = silent_clients.emplace_back(std::make_unique<RawHttpClient>());
        ASSERT_TRUE(client->connect());
        ASSERT_TRUE(client->send(announcement));
    }

    RawHttpClient barrier;
    ASSERT_TRUE(barrier.connect());
    ASSERT_TRUE(barrier.send("GET /api/ping HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    auto pong = barrier.read_response();
    ASSERT_TRUE(pong.has_value());
    EXPECT_EQ(pong->content().cpp_str(), "pong");

    EXPECT_LT(sihd::sys::os::current_rss() - rss_before, allowed_growth);
}

} // namespace test
