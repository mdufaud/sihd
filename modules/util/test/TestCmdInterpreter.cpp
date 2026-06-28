#include <gtest/gtest.h>

#include <sihd/util/CmdInterpreter.hpp>
#include <sihd/util/Splitter.hpp>

namespace test
{
using namespace sihd::util;

class TestCmdInterpreter: public ::testing::Test
{
    protected:
        TestCmdInterpreter() = default;
        virtual ~TestCmdInterpreter() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestCmdInterpreter, test_cmd_handle)
{
    std::string last_cmd;
    std::vector<std::string> last_args;

    CmdInterpreter interp({}, Splitter(' '));
    interp.set_cmd("hello", [&](const std::vector<std::string> & args) {
        last_cmd = "hello";
        last_args = args;
    });

    EXPECT_TRUE(interp.handle("hello world foo"));
    EXPECT_EQ(last_cmd, "hello");
    ASSERT_EQ(last_args.size(), 2u);
    EXPECT_EQ(last_args[0], "world");
    EXPECT_EQ(last_args[1], "foo");

    EXPECT_FALSE(interp.handle("unknown cmd"));
    EXPECT_FALSE(interp.handle(""));
}

TEST_F(TestCmdInterpreter, test_cmd_history)
{
    CmdInterpreter interp;
    interp.set_max_history(3);

    int count = 0;
    interp.set_cmd("go", [&](const std::vector<std::string> &) { count++; });

    interp.handle("go");
    interp.handle("go");
    interp.handle("go");
    interp.handle("go");

    EXPECT_EQ(count, 4);

    auto & hist = interp.history();
    EXPECT_EQ(hist.size(), 3u);
    EXPECT_EQ(hist.front(), "go");
}

TEST_F(TestCmdInterpreter, test_cmd_set_max_history_truncates)
{
    CmdInterpreter interp;
    interp.set_max_history(10);

    interp.set_cmd("x", [](const std::vector<std::string> &) {});

    for (int i = 0; i < 10; ++i)
        interp.handle("x");

    EXPECT_EQ(interp.history().size(), 10u);

    interp.set_max_history(3);
    EXPECT_EQ(interp.history().size(), 3u);
}

TEST_F(TestCmdInterpreter, test_cmd_dump_load_history)
{
    CmdInterpreter interp;
    interp.set_max_history(5);

    interp.set_cmd("a", [](const std::vector<std::string> &) {});
    interp.handle("a");
    interp.handle("a");

    auto dump = interp.dump_history();
    EXPECT_EQ(dump.size(), 2u);

    CmdInterpreter interp2;
    interp2.set_max_history(5);
    interp2.set_cmd("a", [](const std::vector<std::string> &) {});
    interp2.load_history(dump);
    EXPECT_EQ(interp2.history().size(), 2u);
}

} // namespace test
