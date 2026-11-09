#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/program.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <fcntl.h>
# include <sys/prctl.h>
# include <unistd.h>
#endif

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestProcessInfo: public ::testing::Test
{
    protected:
        TestProcessInfo() { LoggerManager::stream(); }

        virtual ~TestProcessInfo() { LoggerManager::clear_loggers(); }

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

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
TEST_F(TestProcessInfo, test_processinfo_creation_time_with_spaced_name)
{
    // PR_GET_NAME fills TASK_COMM_LEN bytes
    constexpr size_t comm_size = 16;
    std::array<char, comm_size> previous_name {};
    ASSERT_EQ(prctl(PR_GET_NAME, previous_name.data()), 0);
    const Timestamp expected = ProcessInfo(os::pid()).creation_time();
    ASSERT_EQ(prctl(PR_SET_NAME, "with space"), 0);
    Defer restore([&] { prctl(PR_SET_NAME, previous_name.data()); });

    const Timestamp creation_time = ProcessInfo(os::pid()).creation_time();

    EXPECT_EQ(creation_time.get(), expected.get());
}

TEST_F(TestProcessInfo, test_processinfo_write_into_file_stdin_refused)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string input = fs::combine(tmp_dir.path(), "input.txt");
    ASSERT_TRUE(fs::write(input, "child input file").has_value());
    Process proc({helper_path(), "sleep", "10"});
    ASSERT_TRUE(proc.stdin_from_file(input));
    ASSERT_TRUE(proc.execute());

    const bool written = ProcessInfo(proc.pid()).write_into_stdin(std::string_view("X"));

    EXPECT_TRUE(proc.terminate());
    EXPECT_FALSE(written);
    EXPECT_EQ(fs::read_all(input).value_or(""), "child input file");
}

TEST_F(TestProcessInfo, test_processinfo_write_into_pipe_stdin)
{
    std::array<int, 2> fds {};
    ASSERT_EQ(pipe2(fds.data(), O_CLOEXEC), 0);
    std::string output;
    Process proc({"cat"});
    proc.stdin_from(fds[0]);
    proc.stdout_to(output);
    ASSERT_TRUE(proc.execute());

    const bool written = ProcessInfo(proc.pid()).write_into_stdin(std::string_view("hello"));
    close(fds[1]);
    ASSERT_TRUE(proc.wait_exit().value_or(false));
    EXPECT_TRUE(proc.terminate());

    EXPECT_TRUE(written);
    EXPECT_EQ(output, "hello");
}
#endif

} // namespace test
