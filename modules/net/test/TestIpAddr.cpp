#include <sys/types.h>

#include <bit>
#include <cstring>

#include <gtest/gtest.h>

#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/in.h>
# include <sys/socket.h>
#else
# include <winsock2.h>
#endif

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

using namespace sihd::util;

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::net;
class TestIpAddr: public ::testing::Test
{
    protected:
        TestIpAddr() { LoggerManager::stream(); }

        virtual ~TestIpAddr() { LoggerManager::clear_loggers(); }

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
    EXPECT_FALSE(IpAddr("google.com").fetch_hostname().has_value());

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
    auto dns_result = dns::lookup("google.com");
    if (!dns_result.has_value())
        GTEST_SKIP() << "no internet: " << dns_result.error().message;

    SIHD_LOG(debug, "Nb IPs: {}", dns_result->results.size());
    fmt::print("{}\n", dns_result->dump());
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_error)
{
    auto dns_result = dns::lookup("notgoogleatall.dot.com");
    ASSERT_FALSE(dns_result.has_value());
    EXPECT_EQ(dns_result.error().code, ErrorCode::not_found);
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_local)
{
    auto dns_result = dns::lookup("localhost");
    ASSERT_TRUE(dns_result.has_value());
    EXPECT_FALSE(dns_result->results.empty());
    fmt::print("{}\n", dns_result->dump());
    EXPECT_EQ(dns_result->host, "localhost");

    const IpAddr & first_ip = dns_result->results.at(0).addr;
    if (first_ip.is_ipv6())
        EXPECT_EQ(first_ip.str(), "::1");
    else
        EXPECT_EQ(first_ip.str(), "127.0.0.1");

    dns_result = dns::lookup("127.0.0.1");
    ASSERT_TRUE(dns_result.has_value());
    EXPECT_FALSE(dns_result->results.empty());
    fmt::print("{}\n", dns_result->dump());
    EXPECT_EQ(dns_result->host, "127.0.0.1");
    EXPECT_EQ(dns_result->results.at(0).addr.str(), "127.0.0.1");
    EXPECT_EQ(dns_result->results.at(0).addr.is_ipv6(), false);
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
    EXPECT_TRUE(ip::is_private_ipv4(0x64400001));  // 100.64.0.1
    EXPECT_TRUE(ip::is_private_ipv4(0x646464C8));  // 100.100.100.200
    EXPECT_FALSE(ip::is_private_ipv4(0x643FFFFF)); // 100.63.255.255
    EXPECT_FALSE(ip::is_private_ipv4(0x64800000)); // 100.128.0.0
    EXPECT_TRUE(ip::is_private_ipv4(0xC6120001));  // 198.18.0.1
    EXPECT_TRUE(ip::is_private_ipv4(0xC613FFFF));  // 198.19.255.255
    EXPECT_FALSE(ip::is_private_ipv4(0xC6140000)); // 198.20.0.0
    EXPECT_TRUE(ip::is_private_ipv4(0xC0000001));  // 192.0.0.1
    EXPECT_FALSE(ip::is_private_ipv4(0xC0000101)); // 192.0.1.1
    EXPECT_TRUE(ip::is_private_ipv4(0xE0000001));  // 224.0.0.1
    EXPECT_TRUE(ip::is_private_ipv4(0xFFFFFFFF));  // 255.255.255.255
    EXPECT_FALSE(ip::is_private_ipv4(0xDFFFFFFF)); // 223.255.255.255
}

TEST_F(TestIpAddr, test_is_private_host)
{
    EXPECT_EQ(ip::is_private_host("localhost"), true);
    EXPECT_EQ(ip::is_private_host("127.0.0.1"), true);
    EXPECT_EQ(ip::is_private_host("100.100.100.200"), true);
    EXPECT_EQ(ip::is_private_host("8.8.8.8"), false);
}

TEST_F(TestIpAddr, test_is_private_host_ipv6)
{
    EXPECT_EQ(ip::is_private_host("::1"), true);
    EXPECT_EQ(ip::is_private_host("::"), true);
    EXPECT_EQ(ip::is_private_host("fe80::1"), true);
    EXPECT_EQ(ip::is_private_host("fd00:ec2::254"), true);
    EXPECT_EQ(ip::is_private_host("fc00::1"), true);
    EXPECT_EQ(ip::is_private_host("ff02::1"), true);
    EXPECT_EQ(ip::is_private_host("::ffff:127.0.0.1"), true);
    EXPECT_EQ(ip::is_private_host("::ffff:169.254.169.254"), true);
    EXPECT_EQ(ip::is_private_host("64:ff9b::7f00:1"), true);
    EXPECT_EQ(ip::is_private_host("2002:7f00:1::1"), true);
    EXPECT_EQ(ip::is_private_host("::ffff:8.8.8.8"), false);
    EXPECT_EQ(ip::is_private_host("2001:4860:4860::8888"), false);
}

TEST_F(TestIpAddr, test_is_private_host_unresolvable_is_an_error)
{
    const auto empty = ip::is_private_host("");
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::not_found);

    const auto embedded_nul = ip::is_private_host(std::string("127.0.0.1\0.example.com", 22));
    ASSERT_FALSE(embedded_nul.has_value());
    EXPECT_EQ(embedded_nul.error().code, ErrorCode::not_found);
}

TEST_F(TestIpAddr, test_public_addresses)
{
    const auto v4 = ip::public_addresses("8.8.8.8");
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(*v4, std::vector<std::string>({"8.8.8.8"}));

    const auto v6 = ip::public_addresses("2001:4860:4860::8888");
    ASSERT_TRUE(v6.has_value());
    EXPECT_EQ(*v6, std::vector<std::string>({"2001:4860:4860::8888"}));
}

TEST_F(TestIpAddr, test_public_addresses_refuses_private_and_unresolvable)
{
    const auto loopback = ip::public_addresses("127.0.0.1");
    ASSERT_FALSE(loopback.has_value());
    EXPECT_EQ(loopback.error().code, ErrorCode::permission_denied);

    const auto mapped = ip::public_addresses("::ffff:10.0.0.1");
    ASSERT_FALSE(mapped.has_value());
    EXPECT_EQ(mapped.error().code, ErrorCode::permission_denied);

    const auto empty = ip::public_addresses("");
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::not_found);
}

TEST_F(TestIpAddr, test_ipaddr_dns_lookup_async)
{
    auto future = dns::lookup_async("localhost");
    auto dns_result = future.get();
    ASSERT_TRUE(dns_result.has_value());
    EXPECT_FALSE(dns_result->results.empty());
    EXPECT_EQ(dns_result->host, "localhost");
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
    // requires internet: an unresolvable host is an error, not an empty address
    auto unresolved = dns::find("unresolvable-host-sihd-test");
    EXPECT_FALSE(unresolved.has_value());
}

TEST_F(TestIpAddr, test_ipaddr_subnet_edges)
{
    IpAddr addr("10.0.0.1");
    addr.set_subnet_mask(32);
    // /32 has a zero wildcard: hosts must not underflow
    EXPECT_EQ(addr.subnet().hosts, 0u);

    // subnet math is ipv4 only
    IpAddr v6("2001:db8::1/64");
    Subnet zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    Subnet subnet = v6.subnet();
    EXPECT_EQ(memcmp(&subnet, &zeroed, sizeof(Subnet)), 0);
}

TEST_F(TestIpAddr, test_ipaddr_name_mappings)
{
    EXPECT_EQ(ip::domain("ipv4"), AF_INET);
    EXPECT_EQ(ip::domain("ipv6"), AF_INET6);
    EXPECT_EQ(ip::domain("nope"), -1);
    EXPECT_EQ(ip::domain(""), -1);

    EXPECT_EQ(ip::socktype("tcp"), SOCK_STREAM);
    EXPECT_EQ(ip::socktype("udp"), SOCK_DGRAM);
    EXPECT_EQ(ip::socktype("stream"), SOCK_STREAM);
    EXPECT_EQ(ip::socktype("datagram"), SOCK_DGRAM);
    EXPECT_EQ(ip::socktype("nope"), -1);

    EXPECT_STREQ(ip::domain_str(AF_INET), "ipv4");
    EXPECT_STREQ(ip::domain_str(AF_INET6), "ipv6");
    EXPECT_STREQ(ip::domain_str(-1), "error");
    EXPECT_STREQ(ip::domain_str(123), "");

    EXPECT_STREQ(ip::socktype_str(SOCK_STREAM), "stream");
    EXPECT_STREQ(ip::socktype_str(SOCK_DGRAM), "datagram");
    EXPECT_STREQ(ip::socktype_str(-1), "error");

    EXPECT_EQ(ip::protocol("udp"), IPPROTO_UDP);
    EXPECT_EQ(ip::protocol("tcp"), IPPROTO_TCP);
    EXPECT_EQ(ip::protocol("nope"), -1);
    EXPECT_STREQ(ip::protocol_str(IPPROTO_UDP), "udp");
    EXPECT_STREQ(ip::protocol_str(IPPROTO_TCP), "tcp");
    EXPECT_STREQ(ip::protocol_str(300), "");
}

TEST_F(TestIpAddr, test_ipaddr_netmask_helpers)
{
    EXPECT_EQ(ip::to_netmask(0), 0u);
    EXPECT_EQ(ip::to_netmask(32), 0xffffffffu);
    EXPECT_EQ(ip::to_netmask(33), 0xffffffffu);
    EXPECT_EQ(ip::to_netmask(64), 0xffffffffu);
    for (int bits : {1, 4, 8, 16, 24, 31})
        EXPECT_EQ(std::popcount(ip::to_netmask((uint32_t)bits)), bits) << bits;

    EXPECT_TRUE(ip::is_valid_netmask(0xffffffff));
    EXPECT_TRUE(ip::is_valid_netmask(0xffffff80));
    EXPECT_TRUE(ip::is_valid_netmask(0xffff0000));
    EXPECT_TRUE(ip::is_valid_netmask(0x80000000));
    EXPECT_FALSE(ip::is_valid_netmask(0));
    EXPECT_FALSE(ip::is_valid_netmask(0x7fffff00));
    EXPECT_FALSE(ip::is_valid_netmask(0xff00ff00));
    EXPECT_FALSE(ip::is_valid_netmask(0x00000001));
}

TEST_F(TestIpAddr, test_ipaddr_is_valid_ip_inputs)
{
    EXPECT_TRUE(ip::is_valid_ipv4("255.255.255.255"));
    EXPECT_TRUE(ip::is_valid_ipv4("0.0.0.0"));
    EXPECT_FALSE(ip::is_valid_ipv4("256.0.0.1"));
    EXPECT_FALSE(ip::is_valid_ipv4("1.2.3"));
    EXPECT_FALSE(ip::is_valid_ipv4("1.2.3.4.5"));
    EXPECT_FALSE(ip::is_valid_ipv4("a.b.c.d"));
    EXPECT_FALSE(ip::is_valid_ipv4(""));
    EXPECT_FALSE(ip::is_valid_ipv4("::1"));

    EXPECT_TRUE(ip::is_valid_ipv6("::"));
    EXPECT_TRUE(ip::is_valid_ipv6("::1"));
    EXPECT_TRUE(ip::is_valid_ipv6("2001:db8::1"));
    EXPECT_FALSE(ip::is_valid_ipv6("gggg::1"));
    EXPECT_FALSE(ip::is_valid_ipv6("2001:db8:::1"));
    EXPECT_FALSE(ip::is_valid_ipv6("2001:db8::1:2:3:4:5:6:7"));
    EXPECT_FALSE(ip::is_valid_ipv6(""));

    EXPECT_FALSE(ip::is_valid_ip(""));
    EXPECT_FALSE(ip::is_valid_ip("toto"));
}

TEST_F(TestIpAddr, test_dns_find_filters)
{
    auto empty = dns::find("");
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::invalid_argument);

    auto icmp = dns::find("localhost", false, -1, IPPROTO_ICMP);
    ASSERT_FALSE(icmp.has_value());
    EXPECT_EQ(icmp.error().code, ErrorCode::invalid_argument);

    auto icmpv6 = dns::find("localhost", false, -1, IPPROTO_ICMPV6);
    ASSERT_FALSE(icmpv6.has_value());
    EXPECT_EQ(icmpv6.error().code, ErrorCode::invalid_argument);

    auto ipv4 = dns::find("localhost");
    ASSERT_TRUE(ipv4.has_value());
    EXPECT_TRUE(ipv4->is_ipv4());

    auto ipv6 = dns::find("localhost", true);
    ASSERT_TRUE(ipv6.has_value());
    EXPECT_TRUE(ipv6->is_ipv6());

    auto unknown = dns::find("no-such-host-sihd-dns-find");
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
}

TEST_F(TestIpAddr, test_dns_lookup_error_and_dump)
{
    // windows getaddrinfo("") resolves to the local host: the empty-name refusal is unix-only
    if constexpr (!build::is_windows)
    {
        auto failed = dns::lookup("");
        ASSERT_FALSE(failed.has_value());
        EXPECT_EQ(failed.error().code, ErrorCode::not_found);
    }

    auto dns_result = dns::lookup("localhost");
    ASSERT_TRUE(dns_result.has_value());
    EXPECT_FALSE(dns_result->results.empty());

    const std::string dump = dns_result->dump();
    EXPECT_NE(dump.find("host: localhost"), std::string::npos);
    EXPECT_NE(dump.find("ipv"), std::string::npos);
}

} // namespace test
