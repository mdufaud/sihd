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

    TEST_F(TestProcess, test_Process)
    {
        EXPECT_EQ(true, true);
    }
}
