#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Ip.hpp>

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::net;
    class TestIpAddr:   public ::testing::Test
    {
        protected:
            TestIpAddr()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestIpAddr()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void dump_dns(const IpAddr::DnsInfo & dns)
            {
                LOG(info, "hostname: " << dns.hostname);
                for (const IpAddr::IpInfo & info: dns.lst_ip)
                {
                    LOG(info, "ip: " << info.ip
                                << " - socket " << Ip::socktype_to_string(info.socktype)
                                << " - proto " << Ip::protocol_to_string(info.protocol)
                                << " - " << (info.ipv6 ? "ipv6" : "ipv4"));
                }
            }
    };

    TEST_F(TestIpAddr, test_ipaddr_ip_valid)
    {
        EXPECT_TRUE(IpAddr::is_valid_ipv4("127.0.0.1"));
        EXPECT_FALSE(IpAddr::is_valid_ipv6("127.0.0.1"));
        EXPECT_FALSE(IpAddr::is_valid_ip("toto"));
    }

    TEST_F(TestIpAddr, test_ipaddr_constructor)
    {
        TRACE("127.0.0.1 with no dns lookup");
        IpAddr addr_local("127.0.0.1", 0, false);
        EXPECT_EQ(addr_local.hostname(), "");
        this->dump_dns(addr_local.dns());
        EXPECT_EQ(addr_local.get_first_ipv4(), "127.0.0.1");
        EXPECT_EQ(addr_local.get_protocol_ip(Ip::protocol("ip")), "127.0.0.1");

        TRACE("localhost with dns lookup");
        IpAddr addr_dns("localhost");
        EXPECT_EQ(addr_dns.hostname(), "localhost");
        this->dump_dns(addr_dns.dns());
        EXPECT_EQ(addr_dns.get_first_ipv4(), "127.0.0.1");
        EXPECT_EQ(addr_dns.get_protocol_ip(Ip::protocol("tcp")), "127.0.0.1");

        TRACE("127.0.0.1 with dns lookup");
        IpAddr addr_dns_ip("127.0.0.1");
        EXPECT_EQ(addr_dns.hostname(), "localhost");
        this->dump_dns(addr_dns.dns());
        EXPECT_EQ(addr_dns.get_first_ipv4(), "127.0.0.1");
        EXPECT_EQ(addr_dns.get_protocol_ip(Ip::protocol("tcp")), "127.0.0.1");
    }

    TEST_F(TestIpAddr, test_ipaddr_ip_name)
    {
        EXPECT_EQ(IpAddr::fetch_ip_name("127.0.0.1"), "localhost");
        EXPECT_EQ(IpAddr::fetch_ip_name("google.com"), "");
        // requires internet
        TRACE(IpAddr::fetch_ip_name("216.58.215.46"));
        TRACE(IpAddr::fetch_ip_name("2a00:1450:4007:810::200e"));
    }

    TEST_F(TestIpAddr, test_ipaddr_dns_lookup_google)
    {
        // requires internet
        IpAddr google("google.com");

        this->dump_dns(google.dns());
        const IpAddr::DnsInfo & info = google.dns();
        TRACE("Nb IPs: " << info.lst_ip.size());
        TRACE("Hostname: " << info.hostname);
        std::string google_proto_udp = google.get_protocol_ip(Ip::protocol("udp"));
        TRACE("Google first IPV4: " << google.get_first_ipv4());
        TRACE("Google first IPV6: " << google.get_first_ipv6());
        TRACE("Google IP IPV4: " << google.get_protocol_ip(Ip::protocol("ip")));
        TRACE("Google UDP IPV4: " << google_proto_udp);
        TRACE("Google TCP IPV4: " << google.get_protocol_ip(Ip::protocol("tcp")));
        TRACE("Google sock stream IPV4: " << google.get_socktype_ip(Ip::socktype("stream")));
        TRACE("Google sock datagram IPV4: " << google.get_socktype_ip(Ip::socktype("datagram")));
        TRACE("Google sock raw IPV6: " << google.get_socktype_ip(Ip::socktype("raw"), true));
    }
    
    TEST_F(TestIpAddr, test_ipaddr_dns_lookup_error)
    {
        auto dns = IpAddr::dns_lookup("notgoogleatall.dot.com", true);
        EXPECT_FALSE(dns.has_value());
    }

    TEST_F(TestIpAddr, test_ipaddr_dns_lookup_local)
    {
        auto dns = IpAddr::dns_lookup("localhost", true);
        EXPECT_TRUE(dns.has_value());
        this->dump_dns(dns.value());
        IpAddr::DnsInfo & info = dns.value();
        EXPECT_EQ(info.hostname, "localhost");
        EXPECT_TRUE(info.lst_ip.size() > 0);
        EXPECT_EQ(info.lst_ip[0].ip, "127.0.0.1");
        EXPECT_EQ(info.lst_ip[0].ipv6, false);

        dns = IpAddr::dns_lookup("127.0.0.1", true);
        EXPECT_TRUE(dns.has_value());
        this->dump_dns(dns.value());
        info = dns.value();
        EXPECT_EQ(info.hostname, "127.0.0.1");
        EXPECT_TRUE(info.lst_ip.size() > 0);
        EXPECT_EQ(info.lst_ip[0].ip, "127.0.0.1");
        EXPECT_EQ(info.lst_ip[0].ipv6, false);
    }
}