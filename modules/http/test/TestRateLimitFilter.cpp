#include <clock_helper.hpp>

#include <gtest/gtest.h>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/RateLimitFilter.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/time.hpp>

#include "http_test_helpers.hpp"

namespace test
{
using namespace sihd::http;
using namespace sihd::util;

TEST(TestRateLimitFilter, test_rate_limit_filter_burst)
{
    ManualClock clock;
    RateLimitFilter filter(2, Duration(time::seconds(1)));
    filter.set_clock(&clock);

    const HttpFilterInfo info {.uri = "/", .method = HttpRequest::Get, .client_ip = "127.0.0.1", .headers = {}};
    EXPECT_TRUE(filter.on_filter_connection(info));
    EXPECT_TRUE(filter.on_filter_connection(info));
    EXPECT_FALSE(filter.on_filter_connection(info));
}

TEST(TestRateLimitFilter, test_rate_limit_filter_per_ip)
{
    RateLimitFilter filter(1, Duration(time::seconds(1)));

    const HttpFilterInfo first {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.1", .headers = {}};
    const HttpFilterInfo second {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.2", .headers = {}};

    EXPECT_TRUE(filter.on_filter_connection(first));
    EXPECT_FALSE(filter.on_filter_connection(first));
    EXPECT_TRUE(filter.on_filter_connection(second));
    EXPECT_FALSE(filter.on_filter_connection(second));
}

TEST(TestRateLimitFilter, test_rate_limit_filter_empty_ip)
{
    RateLimitFilter filter(1, Duration(time::seconds(1)));

    const HttpFilterInfo info {.uri = "/", .method = HttpRequest::Get, .client_ip = "", .headers = {}};
    EXPECT_TRUE(filter.on_filter_connection(info));
    EXPECT_TRUE(filter.on_filter_connection(info));
}

TEST(TestRateLimitFilter, test_rate_limit_filter_unbounded)
{
    ManualClock clock;
    RateLimitFilter filter(1, Duration(time::seconds(1)));
    filter.set_max_entries(0);
    filter.set_clock(&clock);

    for (size_t i = 0; i < 50; ++i)
    {
        const HttpFilterInfo info {.uri = "/",
                                   .method = HttpRequest::Get,
                                   .client_ip = "10.0.0." + std::to_string(i),
                                   .headers = {}};
        EXPECT_TRUE(filter.on_filter_connection(info));
    }
    // every ip is tracked and budgeted even without a bound
    const HttpFilterInfo first {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.0", .headers = {}};
    EXPECT_FALSE(filter.on_filter_connection(first));
}

TEST(TestRateLimitFilter, test_rate_limit_filter_eviction)
{
    ManualClock clock;
    RateLimitFilter filter(1, Duration(time::seconds(1)));
    filter.set_max_entries(2);
    filter.set_clock(&clock);

    const HttpFilterInfo a {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.1", .headers = {}};
    const HttpFilterInfo b {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.2", .headers = {}};
    const HttpFilterInfo c {.uri = "/", .method = HttpRequest::Get, .client_ip = "10.0.0.3", .headers = {}};

    EXPECT_TRUE(filter.on_filter_connection(a));
    // a consumed its budget: advancing past the window refills it to full
    clock.advance(time::seconds(1));
    EXPECT_TRUE(filter.on_filter_connection(b));

    // map is full: the idle (full budget) entry a is evicted for c
    EXPECT_TRUE(filter.on_filter_connection(c));

    // a fresh a is allowed again, b is still tracked and drained
    EXPECT_TRUE(filter.on_filter_connection(a));
    EXPECT_FALSE(filter.on_filter_connection(b));
}

TEST(TestRateLimitFilter, test_rate_limit_filter_server)
{
    ServerScope scope;
    // frozen clock: the third request cannot refill, whatever the machine load
    ManualClock clock;
    RateLimitFilter filter(2, Duration(time::hours(1)));
    filter.set_clock(&clock);

    scope.server.set_http_filter(&filter);
    scope.server._webservice->set_entry_point("limited", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });
    scope.start();

    auto resp = get("localhost:3001/api/limited");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = get("localhost:3001/api/limited");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);

    resp = get("localhost:3001/api/limited");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Forbidden);
}

TEST(TestRateLimitFilter, test_rate_limit_filter_invalid)
{
    EXPECT_THROW(RateLimitFilter(0, Duration(time::seconds(1))), std::invalid_argument);
    EXPECT_THROW(RateLimitFilter(1, Duration(0)), std::invalid_argument);
}

} // namespace test
