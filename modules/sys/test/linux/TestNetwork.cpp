#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/network.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

class TestNetwork: public ::testing::Test
{
    protected:
        TestNetwork() { LoggerManager::stream(); }

        virtual ~TestNetwork() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!network::supported)
            {
                GTEST_SKIP() << "network connectivity not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
        }

        void check_connectivity(uint32_t state, const std::string & expected)
        {
            PrivateDBusClient client;
            ASSERT_TRUE(client.start(_bus.address(), {"connectivity"}));

            auto call = receive_member(_server, "Get");
            ASSERT_TRUE(call.has_value());
            EXPECT_EQ(call->path(), "/org/freedesktop/NetworkManager");
            EXPECT_EQ(read_strings(*call),
                      (std::vector<std::string> {"org.freedesktop.NetworkManager", "Connectivity"}));
            answer_u32_variant(_server, *call, state);

            EXPECT_EQ(client.next_line(), expected);
            EXPECT_EQ(client.finish(), 0);
        }

        PrivateDBus _bus;
        Connection _server;
};

TEST_F(TestNetwork, test_connectivity_states)
{
    ASSERT_TRUE(_server.request_name("org.freedesktop.NetworkManager").has_value());

    this->check_connectivity(0, "unknown");
    this->check_connectivity(1, "none");
    this->check_connectivity(2, "portal");
    this->check_connectivity(3, "limited");
    this->check_connectivity(4, "full");
    this->check_connectivity(9, "unknown");
}

TEST_F(TestNetwork, test_no_network_manager)
{
    PrivateDBusClient client;
    ASSERT_TRUE(client.start(_bus.address(), {"connectivity"}));

    EXPECT_EQ(client.next_line(), error_line(ErrorCode::not_supported, "no NetworkManager on this system"));
    EXPECT_EQ(client.finish(), 1);
}

} // namespace test
