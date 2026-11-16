#include <gtest/gtest.h>

#include <sihd/net/LoggerEndpoint.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestLoggerEndpoint: public ::testing::Test
{
    protected:
        TestLoggerEndpoint() { LoggerManager::stream(); }

        virtual ~TestLoggerEndpoint() { LoggerManager::clear_loggers(); }
};

TEST_F(TestLoggerEndpoint, test_loggerendpoint_ip)
{
    LoggerEndpoint endpoint;

    ASSERT_TRUE(endpoint.set_host("127.0.0.1").has_value());
    ASSERT_TRUE(endpoint.set_port(4242).has_value());

    EXPECT_EQ(endpoint.str(), "127.0.0.1:4242");
    EXPECT_EQ(endpoint.ip_addr(), IpAddr("127.0.0.1", 4242));
    EXPECT_EQ(endpoint.set_unix_path("collector.sock").error().message, "unix_path and host/port are exclusive");
}

TEST_F(TestLoggerEndpoint, test_loggerendpoint_unix_path)
{
    LoggerEndpoint endpoint;

    ASSERT_TRUE(endpoint.set_unix_path("collector.sock").has_value());

    EXPECT_EQ(endpoint.str(), "collector.sock");
    EXPECT_EQ(endpoint.set_host("127.0.0.1").error().message, "host and unix_path are exclusive");
    EXPECT_EQ(endpoint.set_port(4242).error().message, "port and unix_path are exclusive");
}

TEST_F(TestLoggerEndpoint, test_loggerendpoint_refuses_invalid_values)
{
    LoggerEndpoint endpoint;

    EXPECT_EQ(endpoint.set_host("not an ip").error().message, "host is not an ip: 'not an ip'");
    EXPECT_EQ(endpoint.set_port(70000).error().message, "invalid port 70000");
    EXPECT_EQ(endpoint.set_port(-1).error().message, "invalid port -1");
    EXPECT_EQ(endpoint.set_unix_path("").error().message, "empty unix_path");
    EXPECT_EQ(endpoint.str(), ":0");
}

} // namespace test
