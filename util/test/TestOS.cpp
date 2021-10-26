#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Task.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestOs:   public ::testing::Test,
                    public IRunnable
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

            bool    run()
            {
                ++ran;
                return true;
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
        auto task = new Task(this);
        EXPECT_TRUE(OS::add_signal_handler(sig, task));
        EXPECT_EQ(this->ran, 0);
        raise(sig);
        EXPECT_EQ(this->ran, 1);
        raise(sig);
        EXPECT_EQ(this->ran, 2);
        EXPECT_TRUE(OS::clear_signal_handler(sig, task));
        delete task;
    }
}
