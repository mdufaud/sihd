#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Handler.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestOs:   public ::testing::Test,
                    public IHandler<int>
    {
        protected:
            TestOs()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestOs()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                OS::clear_signal_handlers();
            }

            virtual void TearDown()
            {
            }

            void handle(int sig)
            {
                (void)sig;
                ++ran;
            }

            int ran = 0;
    };

    TEST_F(TestOs, test_os_backtrace)
    {
        ssize_t size = OS::backtrace(1);
        EXPECT_GE(size, 1);
    }

    TEST_F(TestOs, test_os_signal)
    {
        this->ran = 0;
        int sig = SIGINT;
        Handler<int> handler = Handler<int>(this);
        EXPECT_TRUE(OS::add_signal_handler(sig, &handler));
        EXPECT_EQ(this->ran, 0);
        raise(sig);
        EXPECT_EQ(this->ran, 1);
        raise(sig);
        EXPECT_EQ(this->ran, 2);
        EXPECT_TRUE(OS::clear_signal_handler(sig, &handler));
    }

    TEST_F(TestOs, test_os_resources)
    {
        size_t max_fds = OS::max_fds();
        // in bytes
        size_t peak = OS::peak_rss();
        size_t current = OS::current_rss();

        SIHD_LOG(info, "Peak rss: {} mb", peak / (1024 * 1024));
        SIHD_LOG(info, "Current rss: {} mb", current / (1024 * 1024));
        SIHD_LOG(info, "Max file descriptors: {}", max_fds);

        EXPECT_TRUE(peak > 0);
        EXPECT_TRUE(current > 0);
        EXPECT_TRUE(max_fds > 2);
    }
}
