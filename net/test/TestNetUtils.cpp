#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/utils.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
class TestNetUtils: public ::testing::Test
{
    protected:
        TestNetUtils() { sihd::util::LoggerManager::basic(); }

        virtual ~TestNetUtils() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNetUtils, test_netutils)
{
    Socket sock;

    ASSERT_TRUE(sock.open(AF_INET, SOCK_DGRAM, 0));
    int idx = utils::interface_idx(sock, "lo");
    ASSERT_GT(idx, -1);
    std::string name;
    EXPECT_TRUE(utils::get_interface_name(sock, idx, name));
    EXPECT_EQ(name, "lo");

    SIHD_LOG(debug, "Getting interface 'lo' addrs");
    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));

    EXPECT_TRUE(utils::get_interface_mac(sock, "lo", &in));
    SIHD_LOG(debug, "Mac: {}", IpAddr::ip_str(in));
    EXPECT_NE(IpAddr::ip_str(in), "");

    EXPECT_TRUE(utils::get_interface_addr(sock, "lo", &in));
    SIHD_LOG(debug, "Addr: {}", IpAddr::ip_str(in));
    EXPECT_NE(IpAddr::ip_str(in), "");

    EXPECT_TRUE(utils::get_interface_broadcast(sock, "lo", &in));
    SIHD_LOG(debug, "Broadcast: {}", IpAddr::ip_str(in));
    EXPECT_NE(IpAddr::ip_str(in), "");

    EXPECT_TRUE(utils::get_interface_netmask(sock, "lo", &in));
    SIHD_LOG(debug, "Netmask: {}", IpAddr::ip_str(in));
    EXPECT_NE(IpAddr::ip_str(in), "");
}
} // namespace test
