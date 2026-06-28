#include <gtest/gtest.h>

#include <sihd/sys/proc.hpp>
#include <sihd/util/build.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

std::vector<std::string> cmd_echo(const std::string & arg)
{
    if constexpr (sihd::util::build::is_windows)
        return {"cmd", "/c", "echo", arg};
    else
        return {"echo", arg};
}

std::vector<std::string> cmd_true()
{
    if constexpr (sihd::util::build::is_windows)
        return {"cmd", "/c", "exit", "0"};
    else
        return {"true"};
}

std::vector<std::string> cmd_false()
{
    if constexpr (sihd::util::build::is_windows)
        return {"cmd", "/c", "exit", "1"};
    else
        return {"false"};
}

std::vector<std::string> cmd_bad_path()
{
    if constexpr (sihd::util::build::is_windows)
        return {"cmd", "/c", "dir", "Z:\\nonexistent_path_12345"};
    else
        return {"ls", "/nonexistent_path_12345"};
}

std::vector<std::string> cmd_stdin_filter()
{
    if constexpr (sihd::util::build::is_windows)
        return {"sort"};
    else
        return {"cat"};
}

} // namespace

class TestProc: public ::testing::Test
{
    protected:
        TestProc() = default;
        virtual ~TestProc() = default;
        virtual void SetUp()
        {
            if constexpr (!proc::supported)
            {
                GTEST_SKIP() << "subprocess (fork/exec) not supported on this platform";
            }
        }
        virtual void TearDown() {}
};

TEST_F(TestProc, test_proc_execute_simple)
{
    auto future = proc::execute(cmd_echo("hello"));
    int ret = future.get();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestProc, test_proc_execute_stdout)
{
    std::string output;

    auto future = proc::execute(cmd_echo("test_output"),
                                proc::Options {.stdout_callback = [&output](std::string_view sv) { output += sv; }});

    int ret = future.get();
    EXPECT_EQ(ret, 0);
    EXPECT_NE(output.find("test_output"), std::string::npos);
}

TEST_F(TestProc, test_proc_execute_stderr)
{
    std::string err;

    auto future = proc::execute(cmd_bad_path(),
                                proc::Options {.stderr_callback = [&err](std::string_view sv) { err += sv; }});

    int ret = future.get();
    EXPECT_NE(ret, 0);
    EXPECT_FALSE(err.empty());
}

TEST_F(TestProc, test_proc_execute_exit_code)
{
    auto future = proc::execute(cmd_false());
    EXPECT_NE(future.get(), 0);

    auto future2 = proc::execute(cmd_true());
    EXPECT_EQ(future2.get(), 0);
}

TEST_F(TestProc, test_proc_execute_stdin)
{
    std::string output;

    auto future = proc::execute(cmd_stdin_filter(),
                                proc::Options {
                                    .to_stdin = "hello from stdin",
                                    .stdout_callback = [&output](std::string_view sv) { output += sv; },
                                });

    int ret = future.get();
    EXPECT_EQ(ret, 0);
    EXPECT_NE(output.find("hello from stdin"), std::string::npos);
}

} // namespace test
