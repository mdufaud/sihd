#include <algorithm>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/program.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

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

        virtual void SetUp()
        {
            if constexpr (!ProcessInfo::supported)
            {
                GTEST_SKIP() << "process introspection not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestProcessInfo, test_processinfo)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Does not work under valgrind";

    ProcessInfo pi(getpid());

    ASSERT_EQ(pi.pid(), getpid());
    if constexpr (build::is_windows)
    {
        EXPECT_EQ(pi.name(), "sihd_sys.exe");
    }
    else
    {
        EXPECT_EQ(pi.name(), "sihd_sys");
    }
    EXPECT_FALSE(pi.cwd().empty());
    EXPECT_FALSE(pi.exe_path().empty());
    EXPECT_FALSE(pi.cmd_line().empty());
    EXPECT_FALSE(pi.env().empty());
}

TEST_F(TestProcessInfo, test_processinfo_metrics)
{
    if (os::is_run_by_qemu())
        GTEST_SKIP() << "proc metrics are not faithful under qemu";

    ProcessInfo pi(getpid());

    ProcessMetrics metrics = pi.metrics();

    EXPECT_EQ(metrics.pid, getpid());
    if constexpr (build::is_windows)
        // wine reports no parent process
        EXPECT_GE(metrics.parent_pid, 0);
    else
        EXPECT_GT(metrics.parent_pid, 0);
    EXPECT_NE(metrics.state, ProcessState::unknown);
    EXPECT_GT(metrics.rss_bytes, 0u);
    EXPECT_GT(metrics.virtual_bytes, 0u);
    EXPECT_GE(metrics.thread_count, 1u);
    EXPECT_GE(metrics.user_time.nanoseconds(), 0);
    EXPECT_GE(metrics.system_time.nanoseconds(), 0);
    EXPECT_GE(metrics.fd_count, 0);

    // one read then one write must tick the matching io counters (wine does not account them)
    if constexpr (build::is_windows == false)
    {
        const uint64_t read_ops = metrics.io_read_ops;
        const uint64_t write_ops = metrics.io_write_ops;
        TmpDir tmp_dir;
        ASSERT_TRUE(tmp_dir);
        (void)fs::read_all(program::path());
        EXPECT_TRUE(fs::write(fs::combine(tmp_dir.path(), "io_probe"), "probe").has_value());
        metrics = pi.metrics();
        EXPECT_GT(metrics.io_read_ops, read_ops);
        EXPECT_GT(metrics.io_write_ops, write_ops);
    }
}

TEST_F(TestProcessInfo, test_processinfo_pids)
{
    std::vector<int> pids = ProcessInfo::pids();

    EXPECT_FALSE(pids.empty());
    EXPECT_NE(std::find(pids.begin(), pids.end(), getpid()), pids.end());
}

} // namespace test
