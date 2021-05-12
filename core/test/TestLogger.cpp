#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/core/BasicLogger.hpp>
#include <sihd/core/LevelFilterLogger.hpp>

#include <sihd/core/thread.hpp>

namespace test
{
    using namespace sihd::core;

    NEW_LOGGER("test::logger");

    class TestLogger:   public ::testing::Test
    {
        protected:
            TestLogger()
            {}
            virtual ~TestLogger()
            {}
            virtual void SetUp()
            {
                LoggerManager::clear_loggers();
                LoggerManager::clear_filters();
            }
            virtual void TearDown()
            {}
    };

    TEST_F(TestLogger, test_logging)
    {
        auto logger = new BasicLogger();
        LoggerManager::add(logger);
        LOG(debug, "DEBUG");
        LOG(info, "INFO");
        LOG(warning, "WARNING");
        LOG(error, "ERROR");
        LOG(critical, "CRITICAL");

        TRACE("TEST TRACE");

        LOG(info, "Stream test: " << 1 << " - " << 0.2345);
        LOG_INFO("Format test: %d", 2);
        thread::set_name("new-main-thread-name");
        LOG_INFO("Format test with changed thread name: %d %d %d", 3, 4, 5);
        LoggerManager::clear_loggers();
        LOG_INFO("No printo");
    }

    TEST_F(TestLogger, test_filters)
    {
        auto logger = new BasicLogger();
        LoggerManager::add(logger);
        logger->add_filter(new LevelFilterLogger(LogLevel::warning));
        LOG(error, "Should print");
        LOG(warning, "Should print");
        LOG(info, "Should not print");
        LOG(debug, "Should not print");
        logger->delete_filters();
        LoggerManager::filter(new LevelFilterLogger("CRITICAL"));
        LOG(critical, "Should print");
        LOG(error, "Should not print");
    }
}
