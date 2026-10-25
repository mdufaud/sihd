#include <gtest/gtest.h>

#include <sihd/net/dns.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
using namespace sihd::sys;
class TestDnsServers: public ::testing::Test
{
    protected:
        TestDnsServers() { LoggerManager::stream(); }

        virtual ~TestDnsServers() { LoggerManager::clear_loggers(); }
};

TEST_F(TestDnsServers, test_dns_servers_query_succeeds)
{
    if constexpr (build::is_android)
        GTEST_SKIP() << "android has no resolv.conf";
    if (!build::is_windows && !fs::exists("/etc/resolv.conf"))
        GTEST_SKIP() << "no /etc/resolv.conf";

    auto servers = dns::servers();

    ASSERT_TRUE(servers.has_value()) << servers.error().message;
    for (const dns::DnsServer & server : *servers)
        EXPECT_TRUE(server.addr.has_ip());
}

} // namespace test
