#include <netdb.h> // addrinfo

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::net;
class TestIpAddr: public ::testing::Test
{
    protected:
        TestIpAddr() { sihd::util::LoggerManager::basic(); }

        virtual ~TestIpAddr() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void dump_dns(const IpAddr::DnsInfo & dns)
        {
            SIHD_LOG(info, "hostname: {}", dns.hostname);
            this->dump_ip_lst(dns.lst_ip);
        }

        void dump_ip_lst(const std::vector<IpAddr::IpEntry> & lst_ip)
        {
            for (const IpAddr::IpEntry & info : lst_ip)
            {
                SIHD_LOG(info,
                         "ip: {} - socket {} - proto {} - type {}",
                         info.ip(),
                         ip::socktype_str(info.socktype),
                         ip::protocol_str(info.protocol),
                         info.ipv6 ? "ipv6" : "ipv4");
            }
        }
};

TEST_F(TestIpAddr, test_ipaddr_ip_valid)
{
    EXPECT_TRUE(IpAddr::is_valid_ipv4("127.0.0.1"));
    EXPECT_TRUE(IpAddr::is_valid_ipv6("::1"));
    EXPECT_FALSE(IpAddr::is_valid_ipv6("127.0.0.1"));
    EXPECT_TRUE(IpAddr::is_valid_ip("127.0.0.1"));
    EXPECT_TRUE(IpAddr::is_valid_ip("::1"));
    EXPECT_FALSE(IpAddr::is_valid_ip("toto"));
}

TEST_F(TestIpAddr, test_ipaddr_constructor)
{
    SIHD_LOG(debug, "127.0.0.1 with no dns lookup");
    IpAddr addr_local("127.0.0.1", 0, false);
    EXPECT_EQ(addr_local.hostname(), "");
    this->dump_ip_lst(addr_local.ip_lst());
    EXPECT_EQ(addr_local.first_ipv4_str(), "127.0.0.1");
    EXPECT_EQ(addr_local.protocol_ip_str(ip::protocol("ip")), "127.0.0.1");

    // does not require internet
    SIHD_LOG(debug, "localhost with dns lookup");
    IpAddr addr_dns("localhost", true);
    EXPECT_EQ(addr_dns.hostname(), "localhost");
    this->dump_ip_lst(addr_dns.ip_lst());
    if (addr_dns.ipv4_count() > 0)
    {
        EXPECT_EQ(addr_dns.first_ipv4_str(), "127.0.0.1");
        EXPECT_EQ(addr_dns.protocol_ip_str(ip::protocol("tcp")), "127.0.0.1");
    }
    else
    {
        EXPECT_EQ(addr_dns.first_ipv6_str(), "::1");
        EXPECT_EQ(addr_dns.protocol_ip_str(ip::protocol("tcp"), true), "::1");
    }

    SIHD_LOG(debug, "127.0.0.1 with dns lookup");
    IpAddr addr_dns_ip("127.0.0.1", true);
    EXPECT_EQ(addr_dns.hostname(), "localhost");
    this->dump_ip_lst(addr_dns.ip_lst());
    if (addr_dns.ipv4_count() > 0)
    {
        EXPECT_EQ(addr_dns.first_ipv4_str(), "127.0.0.1");
        EXPECT_EQ(addr_dns.protocol_ip_str(ip::protocol("tcp")), "127.0.0.1");
    }
    else
    {
        EXPECT_EQ(addr_dns.first_ipv6_str(), "::1");
        EXPECT_EQ(addr_dns.protocol_ip_str(ip::protocol("tcp"), true), "::1");
    }
}

TEST_F(TestIpAddr, test_ipaddr_ip_name)
{
    EXPECT_EQ(IpAddr::fetch_ip_name("127.0.0.1"), "localhost");
    EXPECT_EQ(IpAddr::fetch_ip_name("google.com"), "");
    // requires internet
    SIHD_LOG(debug, IpAddr::fetch_ip_name("216.58.215.46"));
    SIHD_LOG(debug, IpAddr::fetch_ip_name("2a00:1450:4007:810::200e"));
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_google)
{
    // requires internet
    IpAddr google("google.com", true);

    this->dump_ip_lst(google.ip_lst());
    SIHD_LOG(debug, "Nb IPs: {}", google.ip_count());
    SIHD_LOG(debug, "Hostname: {}", google.hostname());
    std::string google_proto_udp = google.protocol_ip_str("udp");
    SIHD_LOG(debug, "Google first IPV4: {}", google.first_ipv4_str());
    SIHD_LOG(debug, "Google first IPV6: {}", google.first_ipv6_str());
    SIHD_LOG(debug, "Google IP IPV4: {}", google.protocol_ip_str("ip"));
    SIHD_LOG(debug, "Google UDP IPV4: {}", google_proto_udp);
    SIHD_LOG(debug, "Google TCP IPV4: {}", google.protocol_ip_str("tcp"));
    SIHD_LOG(debug, "Google sock stream IPV4: {}", google.socktype_ip_str("stream"));
    SIHD_LOG(debug, "Google sock datagram IPV4: {}", google.socktype_ip_str("datagram"));
    SIHD_LOG(debug, "Google sock raw IPV6: {}", google.socktype_ip_str("raw", true));
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
    ASSERT_TRUE(info.lst_ip.size() > 0);
    if (info.lst_ip[0].ipv6)
        EXPECT_EQ(info.lst_ip[0].ip(), "::1");
    else
        EXPECT_EQ(info.lst_ip[0].ip(), "127.0.0.1");

    dns = IpAddr::dns_lookup("127.0.0.1", true);
    EXPECT_TRUE(dns.has_value());
    this->dump_dns(dns.value());
    info = dns.value();
    EXPECT_EQ(info.hostname, "127.0.0.1");
    EXPECT_TRUE(info.lst_ip.size() > 0);
    EXPECT_EQ(info.lst_ip[0].ip(), "127.0.0.1");
    EXPECT_EQ(info.lst_ip[0].ipv6, false);
}

TEST_F(TestIpAddr, test_ipaddr_subnet)
{
    IpAddr addr("192.168.10.0/24");
    EXPECT_EQ(addr.subnet_value(), 24u);

    IpAddr test1("192.168.10.3");
    IpAddr test2("100.168.10.3");

    EXPECT_TRUE(addr.is_same_subnet(test1));
    EXPECT_FALSE(addr.is_same_subnet(test2));

    IpAddr same_addr("192.168.10.0");
    same_addr.set_subnet_mask("255.255.255.0");
    EXPECT_EQ(same_addr.subnet_value(), 24u);

    EXPECT_TRUE(same_addr.is_same_subnet(test1));
    EXPECT_FALSE(same_addr.is_same_subnet(test2));

    SIHD_COUT(addr.dump_ip_lst());
    SIHD_COUT(addr.dump_subnet());

    EXPECT_TRUE(memcmp(&addr.subnet(), &same_addr.subnet(), sizeof(IpAddr::Subnet)) == 0);

    EXPECT_EQ(IpAddr::ip_str(addr.subnet().netid), "192.168.10.0");
    EXPECT_EQ(IpAddr::ip_str(addr.subnet().wildcard), "0.0.0.255");
    EXPECT_EQ(IpAddr::ip_str(addr.subnet().netmask), "255.255.255.0");
    EXPECT_EQ(IpAddr::ip_str(addr.subnet().hostmin), "192.168.10.1");
    EXPECT_EQ(IpAddr::ip_str(addr.subnet().hostmax), "192.168.10.254");
    EXPECT_EQ(IpAddr::ip_str(addr.subnet().broadcast), "192.168.10.255");
    EXPECT_EQ(addr.subnet().hosts, 254u);

    IpAddr localhost("localhost", true);
    EXPECT_TRUE(localhost.set_subnet_mask(IpAddr::to_netmask(12)));
    EXPECT_EQ(localhost.subnet_value(), 12u);
    EXPECT_TRUE(localhost.set_subnet_mask(IpAddr::to_netmask(8)));
    EXPECT_EQ(localhost.subnet_value(), 8u);
    // invalid subnet mask 0
    EXPECT_FALSE(localhost.set_subnet_mask(0));
    // invalid subnet mask - should be at least 255.255.255.128
    EXPECT_FALSE(localhost.set_subnet_mask("255.255.255.64"));
}

TEST_F(TestIpAddr, test_ipaddr_ipv6)
{
    IpAddr addr("2001:db8:abcd:0012:0000:0000:0000:0000");
    EXPECT_EQ(addr.first_ipv6_str(), "2001:db8:abcd:12::");
}

} // namespace test
