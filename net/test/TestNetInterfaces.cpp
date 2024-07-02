#include <ifaddrs.h> // getifaddrs
#include <netdb.h>   // addrinfo

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterfaces.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/net/utils.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
class TestNetInterfaces: public ::testing::Test
{
    protected:
        TestNetInterfaces() { sihd::util::LoggerManager::basic(); }

        virtual ~TestNetInterfaces() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNetInterfaces, test_netinterfaces)
{
    NetInterfaces ifs;
    NetIFace *lo = nullptr;

    EXPECT_FALSE(ifs.error());
    std::vector<NetIFace *> ifaces = ifs.ifaces();
    for (NetIFace *iface : ifaces)
    {
        if (iface->name() == "lo")
            lo = iface;
        SIHD_LOG(debug, "Interface: {} -> {}", iface->name(), iface->up() ? "up" : "down");
        SIHD_LOG(debug, "Total addresses: {}", iface->addresses().size());
        SIHD_LOG(debug, "--- IPV4 ---");
        const struct ifaddrs *ifaddr4 = iface->get_addr(AF_INET);
        if (ifaddr4)
        {
            in_addr mask;
            EXPECT_TRUE(iface->get_netmask(ifaddr4, &mask));
            struct sockaddr_in *base = (struct sockaddr_in *)(ifaddr4->ifa_addr);
            struct sockaddr_in netid;
            struct sockaddr_in broadcast;
            utils::fill_sockaddr_network_id(netid, *base, mask);
            utils::fill_sockaddr_broadcast(broadcast, *base, mask);
            SIHD_LOG(debug, "Base ip: {}", ip::to_str(base));
            SIHD_LOG(debug, "Netmask: {}", ip::to_str(&mask));
            SIHD_LOG(debug, "Netid: {}", ip::to_str(&netid));
            SIHD_LOG(debug, "Broadcast: {}", ip::to_str(&broadcast));
        }
        SIHD_LOG(debug, "--- IPV6 ---");
        const struct ifaddrs *ifaddr6 = iface->get_addr(AF_INET6);
        if (ifaddr6)
        {
            in6_addr mask;
            EXPECT_TRUE(iface->get_netmask(ifaddr6, &mask));
            struct sockaddr_in6 *base = (struct sockaddr_in6 *)(ifaddr6->ifa_addr);
            struct sockaddr_in6 netid;
            struct sockaddr_in6 broadcast;
            utils::fill_sockaddr_network_id(netid, *base, mask);
            utils::fill_sockaddr_broadcast(broadcast, *base, mask);
            SIHD_LOG(debug, "Base ip: {}", ip::to_str(base));
            SIHD_LOG(debug, "Netmask: {}", ip::to_str(&mask));
            SIHD_LOG(debug, "Netid: {}", ip::to_str(&netid));
            SIHD_LOG(debug, "Broadcast: {}", ip::to_str(&broadcast));
        }
        std::cout << std::endl;
    }
    EXPECT_TRUE(ifaces.size() > 0);
    EXPECT_NE(lo, nullptr);
    if (lo != nullptr)
    {
        EXPECT_TRUE(lo->loopback());
    }
}
} // namespace test
