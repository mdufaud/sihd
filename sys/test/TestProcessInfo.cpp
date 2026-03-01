#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/util/term.hpp>

#include <sihd/sys/ProcessInfo.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
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
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Does not work under valgrind";

    ProcessInfo pi(getpid());

    ASSERT_EQ(pi.pid(), getpid());
    EXPECT_EQ(pi.name(), "sihd_sys");
    EXPECT_FALSE(pi.cwd().empty());
    EXPECT_FALSE(pi.exe_path().empty());
    EXPECT_FALSE(pi.cmd_line().empty());
    EXPECT_FALSE(pi.env().empty());
}

} // namespace test
