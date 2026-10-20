#include <gtest/gtest.h>

#include <sihd/http/RequestOptions.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/Logger.hpp>

#include "http_test_helpers.hpp"

using enum sihd::util::ErrorCode;

namespace test
{

using namespace sihd;
using namespace sihd::http;
using namespace sihd::util;

class TestRequestGuards: public ::testing::Test
{
    protected:
        TestRequestGuards() { LoggerManager::stream(); }
        virtual ~TestRequestGuards() { LoggerManager::clear_loggers(); }
};

TEST_F(TestRequestGuards, test_resolve_to_pins_the_connection_address)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("res", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("pinned");
    });

    scope.start();

    RequestOptions options;
    options.proxy = "";
    options.resolve = ResolvePin {"pinned.example.invalid", shard_port(3001), {"127.0.0.1"}};
    auto resp = http::get(shard_url("pinned.example.invalid:3001/api/res"), options);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "pinned");
}

TEST_F(TestRequestGuards, test_server_refuses_line_breaks_in_response_headers)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("header_injection", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
        resp.http_header().set_header("x-next", "a\r\nSet-Cookie: injected=1");
    });
    scope.server._webservice->set_entry_point("cookie_injection", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
        resp.set_cookie("sid", "v\r\nSet-Cookie: injected=1");
    });
    scope.server._webservice->set_entry_point("clean", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
        resp.http_header().set_header("x-next", "a");
        resp.set_cookie("sid", "v", "Path=/");
    });

    scope.start();

    auto clean = http::get(shard_url("localhost:3001/api/clean"));
    ASSERT_TRUE(clean.has_value());
    EXPECT_EQ(clean->http_header().find("x-next"), "a");

    for (const std::string & path :
         {shard_url("localhost:3001/api/header_injection"), shard_url("localhost:3001/api/cookie_injection")})
    {
        auto resp = http::get(path);
        ASSERT_FALSE(resp.has_value()) << path;
        EXPECT_EQ(resp.error().code, io_error) << path;
        EXPECT_EQ(resp.error().message,
                  fmt::format("perform '{}': Server returned nothing (no headers, no data)", path))
            << path;
    }
}

} // namespace test
