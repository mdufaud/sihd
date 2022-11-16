#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/net/Pinger.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::net;
    using namespace sihd::util;
    class TestPinger: public ::testing::Test
    {
        protected:
            TestPinger()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestPinger()
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

    TEST_F(TestPinger, test_pinger)
    {
        Pinger pinger("pinger");

        if (pinger.open(false) == false)
        {
            GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                << "execute command: 'sudo setcap cap_net_raw=pe " << FS::executable_path() << "'\n";
        }

        pinger.set_interval(200);
        EXPECT_TRUE(pinger.ping({"8.8.8.8"}, 10));

        auto result = pinger.result();
        SIHD_COUT(result.str());

        EXPECT_EQ(result.transmitted, 10);
        EXPECT_EQ(result.received, 10);
    }

}
