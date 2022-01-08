#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/NetInterfaces.hpp>
#include <sihd/net/IpAddr.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::net;
    class TestNetInterfaces:   public ::testing::Test
    {
        protected:
            TestNetInterfaces()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestNetInterfaces()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestNetInterfaces, test_netinterfaces)
    {
        NetInterfaces ifs;
        NetIFace *lo = nullptr;

        EXPECT_FALSE(ifs.error());
        std::vector<NetIFace *> ifaces = ifs.ifaces();
        for (NetIFace *iface: ifaces)
        {
            if (iface->name() == "lo")
                lo = iface;
            LOG(debug, "Interface: " << iface->name() << " -> " << (iface->up() ? "up" : "down"));
            LOG(debug, "Total addresses: " << iface->addresses().size());
            LOG(debug, "--- IPV4 ---");
            const struct ifaddrs *ifaddr4 = iface->get_addr(AF_INET);
            if (ifaddr4)
            {
                in_addr mask;
                EXPECT_TRUE(iface->get_netmask(ifaddr4, &mask));
                struct sockaddr_in *base = (struct sockaddr_in *)(ifaddr4->ifa_addr);
                struct sockaddr_in netid;
                struct sockaddr_in broadcast;
                IpAddr::fill_sockaddr_network_id(netid, *base, mask);
                IpAddr::fill_sockaddr_broadcast(broadcast, *base, mask);
                LOG(debug, "Base ip: " << IpAddr::ip_to_string(*base));
                LOG(debug, "Netmask: " << IpAddr::ip_to_string(mask));
                LOG(debug, "Netid: " << IpAddr::ip_to_string(netid));
                LOG(debug, "Broadcast: " << IpAddr::ip_to_string(broadcast));
            }
            LOG(debug, "--- IPV6 ---");
            const struct ifaddrs *ifaddr6 = iface->get_addr(AF_INET6);
            if (ifaddr6)
            {
                in6_addr mask;
                EXPECT_TRUE(iface->get_netmask(ifaddr6, &mask));
                struct sockaddr_in6 *base = (struct sockaddr_in6 *)(ifaddr6->ifa_addr);
                struct sockaddr_in6 netid;
                struct sockaddr_in6 broadcast;
                IpAddr::fill_sockaddr_network_id(netid, *base, mask);
                IpAddr::fill_sockaddr_broadcast(broadcast, *base, mask);
                LOG(debug, "Base ip: " << IpAddr::ip_to_string(*base));
                LOG(debug, "Netmask: " << IpAddr::ip_to_string(mask));
                LOG(debug, "Netid: " << IpAddr::ip_to_string(netid));
                LOG(debug, "Broadcast: " << IpAddr::ip_to_string(broadcast));
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
}