#include <optional>

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/util/Logger.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
class TestNetInterface: public ::testing::Test
{
    protected:
        TestNetInterface() { LoggerManager::stream(); }

        virtual ~TestNetInterface() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNetInterface, test_netinterface_loopback)
{
    const std::optional<NetInterface> loopback = find_interface(NetInterface::Kind::loopback, false);
    ASSERT_TRUE(loopback.has_value());

    EXPECT_TRUE(loopback->loopback());
    EXPECT_FALSE(loopback->hardware());
    EXPECT_GT(loopback->index(), 0u);
    EXPECT_GT(loopback->mtu(), 0u);
    EXPECT_EQ(loopback->addr4().str(), "127.0.0.1");
    EXPECT_EQ(loopback->netmask4().str(), "255.0.0.0");
}

TEST_F(TestNetInterface, test_netinterface_kind_str)
{
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::unknown), "unknown");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::loopback), "loopback");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::ethernet), "ethernet");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::wifi), "wifi");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::cellular), "cellular");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::tunnel), "tunnel");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::ppp), "ppp");
    EXPECT_EQ(NetInterface::kind_str(NetInterface::Kind::bridge), "bridge");
}

} // namespace test
