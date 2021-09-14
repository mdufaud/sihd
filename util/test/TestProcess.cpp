#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestProcess:   public ::testing::Test
    {
        protected:
            TestProcess()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestProcess()
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

    TEST_F(TestProcess, test_process)
    {
        Process proc{"ls", "-la"};

        auto status = proc.wait();
        EXPECT_FALSE(status.has_value());
        EXPECT_EQ(proc.return_code(), std::nullopt);
        
        EXPECT_TRUE(proc.run());
        status = proc.wait();
        EXPECT_TRUE(status.has_value());
        EXPECT_TRUE(proc.has_exited());
        EXPECT_EQ(proc.return_code(), 0);
    }
}
