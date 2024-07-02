#include <netdb.h> // addrinfo

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>

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
};

TEST_F(TestIpAddr, test_ipaddr_ip_valid)
{
    EXPECT_TRUE(ip::is_valid_ipv4("127.0.0.1"));
    EXPECT_TRUE(ip::is_valid_ipv6("::1"));
    EXPECT_FALSE(ip::is_valid_ipv6("127.0.0.1"));
    EXPECT_TRUE(ip::is_valid_ip("127.0.0.1"));
    EXPECT_TRUE(ip::is_valid_ip("::1"));
    EXPECT_FALSE(ip::is_valid_ip("toto"));
}

TEST_F(TestIpAddr, test_ipaddr_ip_name)
{
    EXPECT_EQ(IpAddr("127.0.0.1").fetch_name(), "localhost");
    EXPECT_THROW(IpAddr("google.com").fetch_name(), std::invalid_argument);
    // requires internet
    SIHD_LOG(debug, IpAddr("216.58.215.46").fetch_name());
    SIHD_LOG(debug, IpAddr("2a00:1450:4007:810::200e").fetch_name());
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_google)
{
    // requires internet
    auto dns = dns::lookup("google.com");

    SIHD_LOG(debug, "Nb IPs: {}", dns.results.size());
    fmt::print("{}\n", dns.dump());

    //     SIHD_LOG(debug, "Hostname: {}", dns.host);
    //     std::string google_proto_udp = google.protocol_ip_str("udp");
    //     SIHD_LOG(debug, "Google first IPV4: {}", google.first_ipv4_str());
    //     SIHD_LOG(debug, "Google first IPV6: {}", google.first_ipv6_str());
    //     SIHD_LOG(debug, "Google IP IPV4: {}", google.protocol_ip_str("ip"));
    //     SIHD_LOG(debug, "Google UDP IPV4: {}", google_proto_udp);
    //     SIHD_LOG(debug, "Google TCP IPV4: {}", google.protocol_ip_str("tcp"));
    //     SIHD_LOG(debug, "Google sock stream IPV4: {}", google.socktype_ip_str("stream"));
    //     SIHD_LOG(debug, "Google sock datagram IPV4: {}", google.socktype_ip_str("datagram"));
    //     SIHD_LOG(debug, "Google sock raw IPV6: {}", google.socktype_ip_str("raw", true));
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_error)
{
    auto dns = dns::lookup("notgoogleatall.dot.com");
    EXPECT_TRUE(dns.results.empty());
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_local)
{
    auto dns = dns::lookup("localhost");
    EXPECT_FALSE(dns.results.empty());
    fmt::print("{}\n", dns.dump());
    EXPECT_EQ(dns.host, "localhost");
    const IpAddr & first_ip = dns.results.at(0).addr;
    if (first_ip.is_ipv6())
        EXPECT_EQ(first_ip.str(), "::1");
    else
        EXPECT_EQ(first_ip.str(), "127.0.0.1");

    dns = dns::lookup("127.0.0.1");
    EXPECT_FALSE(dns.results.empty());
    fmt::print("{}\n", dns.dump());
    EXPECT_EQ(dns.host, "127.0.0.1");
    EXPECT_EQ(dns.results.at(0).addr.str(), "127.0.0.1");
    EXPECT_EQ(dns.results.at(0).addr.is_ipv6(), false);
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

    // SIHD_COUT(addr.dump_ip_lst());
    SIHD_COUT(addr.dump_subnet());

    EXPECT_EQ(addr.subnet_value(), same_addr.subnet_value());

    Subnet subnet = addr.subnet();
    EXPECT_EQ(ip::to_str(&subnet.netid), "192.168.10.0");
    EXPECT_EQ(ip::to_str(&subnet.wildcard), "0.0.0.255");
    EXPECT_EQ(ip::to_str(&subnet.netmask), "255.255.255.0");
    EXPECT_EQ(ip::to_str(&subnet.hostmin), "192.168.10.1");
    EXPECT_EQ(ip::to_str(&subnet.hostmax), "192.168.10.254");
    EXPECT_EQ(ip::to_str(&subnet.broadcast), "192.168.10.255");
    EXPECT_EQ(subnet.hosts, 254u);

    IpAddr localhost = IpAddr::localhost(0);
    EXPECT_EQ(localhost.str(), "127.0.0.1");
    EXPECT_TRUE(localhost.set_subnet_mask(12));
    EXPECT_EQ(localhost.subnet_value(), 12u);
    EXPECT_TRUE(localhost.set_subnet_mask(8));
    EXPECT_EQ(localhost.subnet_value(), 8u);
    EXPECT_TRUE(localhost.set_subnet_mask(0));
    EXPECT_EQ(localhost.subnet_value(), 0u);
    // invalid subnet mask - should be at least 255.255.255.128
    EXPECT_FALSE(localhost.set_subnet_mask("255.255.255.64"));
}

TEST_F(TestIpAddr, test_ipaddr_str)
{
    IpAddr addr4("123.111.0.44");
    EXPECT_EQ(addr4.str(), "123.111.0.44");

    IpAddr addr6("2001:db8:abcd:0012:0000:0000:0000:0000");
    EXPECT_EQ(addr6.str(), "2001:db8:abcd:12::");

    IpAddr notdns("google.com");
    EXPECT_TRUE(notdns.empty());
    EXPECT_FALSE(notdns.is_ipv4());
    EXPECT_FALSE(notdns.is_ipv6());
}

} // namespace test
