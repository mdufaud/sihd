#include <algorithm>
#include <optional>

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/route.hpp>
#include <sihd/util/Logger.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
class TestRoute: public ::testing::Test
{
    protected:
        TestRoute() { LoggerManager::stream(); }

        virtual ~TestRoute() { LoggerManager::clear_loggers(); }
};

TEST_F(TestRoute, test_route_find_loopback)
{
    const std::optional<NetInterface> loopback = find_interface(NetInterface::Kind::loopback, false);
    ASSERT_TRUE(loopback.has_value());

    auto found = route::find(IpAddr("127.0.0.1"));

    ASSERT_TRUE(found.has_value()) << found.error().message;
    EXPECT_EQ(found->interface_name, loopback->name());
    EXPECT_EQ(found->source.str(), "127.0.0.1");
    EXPECT_FALSE(found->gateway.has_ip());
}

TEST_F(TestRoute, test_route_find_needs_an_address)
{
    auto found = route::find(IpAddr());

    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestRoute, test_route_defaults_are_sorted_by_metric)
{
    auto ipv4_routes = route::defaults_ipv4();
    auto ipv6_routes = route::defaults_ipv6();

    ASSERT_TRUE(ipv4_routes.has_value()) << ipv4_routes.error().message;
    ASSERT_TRUE(ipv6_routes.has_value()) << ipv6_routes.error().message;
    EXPECT_TRUE(std::ranges::is_sorted(*ipv4_routes, {}, &route::Route::metric));
    EXPECT_TRUE(std::ranges::is_sorted(*ipv6_routes, {}, &route::Route::metric));
}

TEST_F(TestRoute, test_route_defaults_name_their_interface)
{
    auto ipv4_routes = route::defaults_ipv4();
    auto ipv6_routes = route::defaults_ipv6();

    ASSERT_TRUE(ipv4_routes.has_value()) << ipv4_routes.error().message;
    ASSERT_TRUE(ipv6_routes.has_value()) << ipv6_routes.error().message;
    for (const route::Route & default_route : *ipv4_routes)
        EXPECT_FALSE(default_route.interface_name.empty());
    for (const route::Route & default_route : *ipv6_routes)
        EXPECT_FALSE(default_route.interface_name.empty());
}

} // namespace test
