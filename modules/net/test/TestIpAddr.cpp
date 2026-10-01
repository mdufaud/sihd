#include <cstring>

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
        TestIpAddr() { sihd::util::LoggerManager::stream(); }

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
    IpAddr localhost("127.0.0.1");
    (void)localhost.fetch_hostname();
    EXPECT_EQ(localhost.hostname(), "localhost");

    // no valid ip inside
    EXPECT_THROW((void)IpAddr("google.com").fetch_hostname(), std::invalid_argument);

    // requires internet
    IpAddr addr1("216.58.215.46");
    if (addr1.fetch_hostname())
    {
        SIHD_LOG(debug, "{}", addr1.hostname());
    }
    IpAddr addr2("2a00:1450:4007:810::200e");
    if (addr2.fetch_hostname())
    {
        SIHD_LOG(debug, "{}", addr2.hostname());
    }
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_google)
{
    // requires internet
    auto dns = dns::lookup("google.com");

    SIHD_LOG(debug, "Nb IPs: {}", dns.results.size());
    fmt::print("{}\n", dns.dump());
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

TEST_F(TestIpAddr, test_ipaddr6_subnet)
{
    IpAddr test1("2001:db8::1/64");
    IpAddr test2("2001:db8::2");
    IpAddr test3("2001:db8::1::1");

    EXPECT_TRUE(test1.is_same_subnet(test2));
    EXPECT_FALSE(test1.is_same_subnet(test3));

    IpAddr test4("2001:db8:1::1/48");
    IpAddr test5("2001:db8:1:2::1");
    IpAddr test6("2001:db8:2::1");
    EXPECT_TRUE(test4.is_same_subnet(test5));
    EXPECT_FALSE(test4.is_same_subnet(test6));

    IpAddr test8("2001:db8::1/128");
    IpAddr test9("2001:db8::1/128");
    IpAddr test10("2001:db8::2/128");
    EXPECT_TRUE(test8.is_same_subnet(test9));
    EXPECT_FALSE(test8.is_same_subnet(test10));
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
    (void)same_addr.set_subnet_mask("255.255.255.0");
    EXPECT_EQ(same_addr.subnet_value(), 24u);

    EXPECT_TRUE(same_addr.is_same_subnet(test1));
    EXPECT_FALSE(same_addr.is_same_subnet(test2));

    SIHD_COUT("{}\n", addr.dump_subnet());

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

TEST_F(TestIpAddr, test_is_private_ipv4)
{
    // 0.0.0.0/8
    EXPECT_TRUE(ip::is_private_ipv4(0x00000000));
    EXPECT_TRUE(ip::is_private_ipv4(0x00FFFFFF));
    // 127.0.0.0/8 loopback
    EXPECT_TRUE(ip::is_private_ipv4(0x7F000001));
    // 10.0.0.0/8
    EXPECT_TRUE(ip::is_private_ipv4(0x0A000001));
    EXPECT_TRUE(ip::is_private_ipv4(0x0AFFFFFF));
    // 172.16.0.0/12
    EXPECT_TRUE(ip::is_private_ipv4(0xAC100001));
    EXPECT_TRUE(ip::is_private_ipv4(0xAC1FFFFF));
    EXPECT_FALSE(ip::is_private_ipv4(0xAC200001));
    // 192.168.0.0/16
    EXPECT_TRUE(ip::is_private_ipv4(0xC0A80001));
    EXPECT_TRUE(ip::is_private_ipv4(0xC0A8FFFF));
    // 169.254.0.0/16 link-local
    EXPECT_TRUE(ip::is_private_ipv4(0xA9FE0001));
    // public
    EXPECT_FALSE(ip::is_private_ipv4(0x08080808)); // 8.8.8.8
    EXPECT_FALSE(ip::is_private_ipv4(0xD83AD32E)); // 216.58.211.46
}

TEST_F(TestIpAddr, test_is_private_host)
{
    EXPECT_TRUE(ip::is_private_host("localhost"));
    EXPECT_TRUE(ip::is_private_host("127.0.0.1"));
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_async)
{
    auto future = dns::lookup_async("localhost");
    auto dns = future.get();
    EXPECT_TRUE(dns.success);
    EXPECT_FALSE(dns.results.empty());
    EXPECT_EQ(dns.host, "localhost");
}

TEST_F(TestIpAddr, test_ipaddr_operators)
{
    IpAddr a("127.0.0.1", 80);
    IpAddr b("127.0.0.1", 80);
    IpAddr c("127.0.0.1", 81);
    IpAddr d("192.168.0.1", 80);
    IpAddr e("::1", 80);

    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a == d);
    EXPECT_FALSE(a == e);

    // move
    IpAddr moved(std::move(a));
    EXPECT_EQ(moved, b);
}

TEST_F(TestIpAddr, test_ipaddr_invalid_mask)
{
    IpAddr v4("10.0.0.1/abc");
    EXPECT_EQ(v4.subnet_value(), 0u);
    EXPECT_TRUE(v4.is_ipv4());

    IpAddr v4_big("10.0.0.1/200");
    EXPECT_EQ(v4_big.subnet_value(), 0u);

    IpAddr v6_big("2001:db8::1/129");
    EXPECT_EQ(v6_big.subnet_value(), 0u);
    EXPECT_TRUE(v6_big.is_ipv6());

    IpAddr addr_v4("10.0.0.1");
    EXPECT_TRUE(addr_v4.set_subnet_mask(24));
    EXPECT_FALSE(addr_v4.set_subnet_mask(33));
    // a failed set keeps the previous mask
    EXPECT_EQ(addr_v4.subnet_value(), 24u);
    EXPECT_TRUE(addr_v4.set_subnet_mask(32));

    IpAddr v6("2001:db8::1");
    EXPECT_TRUE(v6.set_subnet_mask(64));
    EXPECT_FALSE(v6.set_subnet_mask(129));
    EXPECT_EQ(v6.subnet_value(), 64u);
    EXPECT_TRUE(v6.set_subnet_mask(128));

    IpAddr empty;
    EXPECT_FALSE(empty.set_subnet_mask(24));
    EXPECT_FALSE(empty.has_subnet());

    IpAddr a("10.0.0.1/24");
    EXPECT_EQ(a.subnet_value(), 24u);
    IpAddr b("::1");
    EXPECT_FALSE(a.is_same_subnet(b.addr6()));
}

TEST_F(TestIpAddr, test_ipaddr_assign_empty)
{
    IpAddr fallback("127.0.0.1", 80);
    fallback = IpAddr();
    EXPECT_TRUE(fallback.empty());
    EXPECT_FALSE(fallback.has_ip());

    // via a reference: a direct self-assign would warn
    IpAddr self("10.0.0.1");
    IpAddr & self_ref = self;
    self = self_ref;
    EXPECT_TRUE(self == IpAddr("10.0.0.1"));

    fallback = IpAddr("10.0.0.1", 80);
    EXPECT_TRUE(fallback.has_ip());
    // requires internet
    fallback = dns::find("unresolvable-host-sihd-test");
    EXPECT_TRUE(fallback.empty());
}

TEST_F(TestIpAddr, test_ipaddr_subnet_edges)
{
    IpAddr a("10.0.0.1");
    a.set_subnet_mask(32);
    // /32 has a zero wildcard: hosts must not underflow
    EXPECT_EQ(a.subnet().hosts, 0u);

    // subnet math is ipv4 only
    IpAddr v6("2001:db8::1/64");
    Subnet zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    Subnet sub = v6.subnet();
    EXPECT_EQ(memcmp(&sub, &zeroed, sizeof(Subnet)), 0);
}

} // namespace test
