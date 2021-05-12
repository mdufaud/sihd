#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/core/BasicLogger.hpp>

#include <sihd/core/thread.hpp>

namespace test
{
    NEW_LOGGER("test::logger");
    using namespace sihd::core;
    class TestLogger:   public ::testing::Test
    {
        protected:
            TestLogger()
            {}
            virtual ~TestLogger()
            {}
            virtual void SetUp()
            {}
            virtual void TearDown()
            {}
    };

    TEST_F(TestLogger, test_Logger)
    {
        LoggerManager::get()->add_logger(new BasicLogger());
        LOG(info, "This is a test: " << 1 << " - " << 0.2345);
        LOG_INFO("This is a test: %d", 2);
        thread::set_name("mdr");
        LOG_INFO("This is a test: %d %d %d", 3, 4, 5);
    }
}
