#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::sys;
using namespace sihd::util;

TEST(TestLoggerSystem, test_multi_instance)
{
    try
    {
        {
            LoggerSystem first("sihd_test_first");
            LoggerSystem second("sihd_test_second");
            first.log(LogInfo("test::syslog", LogLevel::info), "FIRST");
            second.log(LogInfo("test::syslog", LogLevel::info), "SECOND");
        }
        // the connection re-arms once every instance is gone
        LoggerSystem third("sihd_test_third");
        third.log(LogInfo("test::syslog", LogLevel::info), "THIRD");
    }
    catch (const std::exception & e)
    {
        FAIL() << e.what();
    }
}

} // namespace test
