#include <gtest/gtest.h>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerFilterLevel.hpp>
#include <sihd/util/LoggerFilterSource.hpp>
#include <sihd/util/LoggerStream.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
class LogCounter: public ALogger
{
    public:
        LogCounter() {};
        ~LogCounter() {};

        int debug = 0;
        int info = 0;
        int warning = 0;
        int error = 0;
        int critical = 0;

        std::string msg;
        std::string src;

        virtual void log(const LogInfo & info, std::string_view msg) override
        {
            this->msg = msg;
            this->src = info.source;
            if (info.level == LogLevel::debug)
                ++this->debug;
            else if (info.level == LogLevel::info)
                ++this->info;
            else if (info.level == LogLevel::warning)
                ++this->warning;
            else if (info.level == LogLevel::error)
                ++this->error;
            else if (info.level == LogLevel::critical)
                ++this->critical;
        };
};

class TestLogger: public ::testing::Test
{
    protected:
        TestLogger() {};
        virtual ~TestLogger() {};

        LogCounter *log_counter = nullptr;

        virtual void SetUp()
        {
            this->log_counter = new LogCounter();
            LoggerManager::add(this->log_counter);
        }

        virtual void TearDown()
        {
            LoggerManager::clear_loggers();
            LoggerManager::clear_filters();
        }

        bool has_logged_every_levels()
        {
            return log_counter->debug > 0 && log_counter->info > 0 && log_counter->warning > 0 && log_counter->error > 0
                   && log_counter->critical > 0;
        }
};

TEST_F(TestLogger, test_logger_basic)
{
    Logger log("test::logger");

    LoggerManager::add(new LoggerStream());
    log.log(sihd::util::LogLevel::info, "test");
    ASSERT_EQ(log_counter->src, "test::logger");
    ASSERT_EQ(log_counter->msg, "test");
    log.debug("debug");
    ASSERT_EQ(log_counter->msg, "debug");
    log.info("info");
    ASSERT_EQ(log_counter->msg, "info");
    log.warning("warning");
    ASSERT_EQ(log_counter->msg, "warning");
    log.error("error");
    ASSERT_EQ(log_counter->msg, "error");
    log.critical("critical");
    ASSERT_EQ(log_counter->msg, "critical");
    log.emergency("emergency");
    ASSERT_TRUE(this->has_logged_every_levels());
}

TEST_F(TestLogger, test_logger_macros)
{
    LoggerManager::add(new LoggerStream());
    SIHD_LOG(debug, "DEBUG");
    SIHD_LOG(info, "INFO");
    SIHD_LOG(warning, "WARNING");
    SIHD_LOG(error, "ERROR");
    SIHD_LOG(critical, "CRITICAL");
    ASSERT_TRUE(this->has_logged_every_levels());
    ASSERT_EQ(log_counter->src, "test");

    SIHD_TRACEL("TEST TRACE");
    ASSERT_EQ(log_counter->debug, 2);

    SIHD_LOG(info, "fmt test: {} - {}", 1.23, "hello");
    ASSERT_EQ(log_counter->msg, "fmt test: 1.23 - hello");

    SIHD_LOG_INFO("int test: {:02} - {}", 2, "world");
    ASSERT_EQ(log_counter->msg, "int test: 02 - world");
    ASSERT_EQ(log_counter->info, 3);
}

TEST_F(TestLogger, test_logger_filters)
{
    auto logger = new LoggerStream(stderr, true);
    LoggerManager::add(logger);
    logger->add_filter(new LoggerFilterLevel(LogLevel::warning));
    log_counter->add_filter(new LoggerFilterLevel(LogLevel::warning));
    SIHD_LOG(error, "Should print");
    ASSERT_EQ(log_counter->error, 1);
    SIHD_LOG(warning, "Should print");
    ASSERT_EQ(log_counter->warning, 1);
    SIHD_LOG(info, "Should not print");
    ASSERT_EQ(log_counter->info, 0);
    SIHD_LOG(debug, "Should not print");
    ASSERT_EQ(log_counter->debug, 0);
    logger->delete_filters();
    log_counter->delete_filters();

    LoggerManager::filter(new LoggerFilterLevel("CRITICAL"));
    SIHD_LOG(critical, "Should print");
    ASSERT_EQ(log_counter->critical, 1);
    SIHD_LOG(error, "Should not print");
    ASSERT_EQ(log_counter->error, 1);
    LoggerManager::clear_filters();

    LoggerManager::filter(new LoggerFilterSource("test"));
    SIHD_LOG(critical, "Should not print");
    ASSERT_EQ(log_counter->critical, 1);
    LoggerManager::clear_filters();

    LoggerManager::filter(new LoggerFilterSource("other"));
    SIHD_LOG(debug, "Should print");
    ASSERT_EQ(log_counter->debug, 1);
}
} // namespace test
