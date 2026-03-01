#include <gtest/gtest.h>
#include <sihd/net/Pinger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
using namespace sihd::sys;
class TestPinger: public ::testing::Test
{
    protected:
        TestPinger() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPinger() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPinger, test_pinger)
{
    Pinger pinger("pinger");

    if (pinger.open(false) == false)
    {
        GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                     << "execute command: 'sudo setcap cap_net_raw=pe " << fs::executable_path() << "'\n";
    }

    pinger.set_interval(200);
    pinger.set_ping_count(10);
    pinger.set_client({"8.8.8.8"});

    ASSERT_TRUE(pinger.start());
    ASSERT_TRUE(pinger.stop());

    auto result = pinger.result();
    SIHD_COUT("{}\n", result.str());

    EXPECT_EQ(result.transmitted, 10);
    EXPECT_EQ(result.received, 10);
}

TEST_F(TestPinger, test_pinger_ipv6)
{
    Pinger pinger("pinger");

    if (pinger.open(true) == false)
    {
        GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                     << "execute command: 'sudo setcap cap_net_raw=pe " << fs::executable_path() << "'\n";
    }

    pinger.set_interval(200);
    pinger.set_ping_count(10);
    pinger.set_client(IpAddr("::1"));

    ASSERT_TRUE(pinger.start());
    ASSERT_TRUE(pinger.stop());

    auto result = pinger.result();
    SIHD_COUT("{}\n", result.str());

    EXPECT_EQ(result.transmitted, 10);
    EXPECT_EQ(result.received, 10);
}

} // namespace test
