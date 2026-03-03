#include <gtest/gtest.h>

#include <sihd/sys/proc.hpp>

namespace test
{
using namespace sihd::sys;

class TestProc: public ::testing::Test
{
    protected:
        TestProc() = default;
        virtual ~TestProc() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestProc, test_proc_execute_simple)
{
    auto future = proc::execute({"echo", "hello"});
    int ret = future.get();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestProc, test_proc_execute_stdout)
{
    std::string output;

    auto future = proc::execute({"echo", "test_output"},
                                proc::Options {.stdout_callback = [&output](std::string_view sv) { output += sv; }});

    int ret = future.get();
    EXPECT_EQ(ret, 0);
    EXPECT_NE(output.find("test_output"), std::string::npos);
}

TEST_F(TestProc, test_proc_execute_stderr)
{
    std::string err;

    auto future = proc::execute({"ls", "/nonexistent_path_12345"},
                                proc::Options {.stderr_callback = [&err](std::string_view sv) { err += sv; }});

    int ret = future.get();
    EXPECT_NE(ret, 0);
    EXPECT_FALSE(err.empty());
}

TEST_F(TestProc, test_proc_execute_exit_code)
{
    auto future = proc::execute({"false"});
    EXPECT_NE(future.get(), 0);

    auto future2 = proc::execute({"true"});
    EXPECT_EQ(future2.get(), 0);
}

TEST_F(TestProc, test_proc_execute_stdin)
{
    std::string output;

    auto future = proc::execute({"cat"},
                                proc::Options {
                                    .to_stdin = "hello from stdin",
                                    .stdout_callback = [&output](std::string_view sv) { output += sv; },
                                });

    int ret = future.get();
    EXPECT_EQ(ret, 0);
    EXPECT_NE(output.find("hello from stdin"), std::string::npos);
}

} // namespace test
