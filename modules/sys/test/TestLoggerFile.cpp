#include <gtest/gtest.h>

#include <sihd/sys/LoggerFile.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

SIHD_NEW_LOGGER("test::loggerfile");

using namespace sihd::util;

namespace test
{
using namespace sihd::sys;

class TestLoggerFile: public ::testing::Test
{
    protected:
        TestLoggerFile() = default;
        virtual ~TestLoggerFile() = default;
        virtual void SetUp() {}
        virtual void TearDown() { LoggerManager::clear_loggers(); }
};

TEST_F(TestLoggerFile, test_loggerfile_write)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "test.log");

    {
        auto *logger = new LoggerFile(path, true, "");
        ASSERT_TRUE(logger->is_open());
        LoggerManager::add(logger);

        SIHD_LOG(info, "test message for log file");

        LoggerManager::clear_loggers();
    }

    auto content = fs::read_all(path);
    ASSERT_TRUE(content.has_value());
    EXPECT_NE(content->find("test message for log file"), std::string::npos);
}

} // namespace test
