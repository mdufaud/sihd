#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/net/NetUtils.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::net;
    using namespace sihd::util;
    class TestNetUtils: public ::testing::Test
    {
        protected:
            TestNetUtils()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::FS::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "net",
                    "netutils"
                });
                _cwd = sihd::util::OS::cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::FS::make_directories(_base_test_dir);
            }

            virtual ~TestNetUtils()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestNetUtils, test_netutils)
    {
        Socket sock;

        ASSERT_TRUE(sock.open(AF_INET, SOCK_DGRAM, 0));
        int idx = NetUtils::interface_idx(sock, "lo");
        ASSERT_GT(idx, -1);
        std::string name;
        EXPECT_TRUE(NetUtils::get_interface_name(sock, idx, name));
        EXPECT_EQ(name, "lo");

        SIHD_LOG(debug, "Getting interface 'lo' addrs");
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));

        EXPECT_TRUE(NetUtils::get_interface_mac(sock, "lo", &in));
        SIHD_LOG(debug, "Mac: " << IpAddr::ip_str(in));
        EXPECT_NE(IpAddr::ip_str(in), "");

        EXPECT_TRUE(NetUtils::get_interface_addr(sock, "lo", &in));
        SIHD_LOG(debug, "Addr: " << IpAddr::ip_str(in));
        EXPECT_NE(IpAddr::ip_str(in), "");

        EXPECT_TRUE(NetUtils::get_interface_broadcast(sock, "lo", &in));
        SIHD_LOG(debug, "Broadcast: " << IpAddr::ip_str(in));
        EXPECT_NE(IpAddr::ip_str(in), "");

        EXPECT_TRUE(NetUtils::get_interface_netmask(sock, "lo", &in));
        SIHD_LOG(debug, "Netmask: " << IpAddr::ip_str(in));
        EXPECT_NE(IpAddr::ip_str(in), "");
    }
}