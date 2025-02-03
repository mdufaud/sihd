#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/util/ProcessInfo.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestProcessInfo: public ::testing::Test
{
    protected:
        TestProcessInfo() { sihd::util::LoggerManager::stream(); }

        virtual ~TestProcessInfo() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestProcessInfo, test_processinfo)
{
    ProcessInfo pi(getpid());

    SIHD_TRACEV(pi.pid());
    SIHD_TRACEV(pi.name());
    // SIHD_TRACEV(pi.exe_path());
}

} // namespace test
