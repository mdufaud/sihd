#include <gtest/gtest.h>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/time.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::ssh;

class TestSshCommand: public ::testing::Test
{
    protected:
        TestSshCommand() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshCommand() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshCommand, test_sshcommand_simple)
{
    auto test_server = make_test_server("test-sshcommand-simple");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    EXPECT_TRUE(session.connected());

    std::string stdout_str;
    std::string stderr_str;
    sihd::util::Handler<std::string_view, bool> test_output_handler(
        [&stdout_str, &stderr_str](std::string_view buf, bool is_stderr) {
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
    SshCommand cmd = session.make_command();
    cmd.output_handler = &test_output_handler;
    EXPECT_TRUE(cmd.execute("echo 'hello world'"));
    EXPECT_TRUE(cmd.wait(time::seconds(5)));
    EXPECT_EQ(stderr_str, "");
    EXPECT_EQ(stdout_str, "hello world\n");
    EXPECT_EQ(cmd.exit_status(), 0);
}

TEST_F(TestSshCommand, test_sshcommand_async)
{
    auto test_server = make_test_server("test-sshcommand-async");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    EXPECT_TRUE(session.connected());

    std::string stdout_str;
    std::string stderr_str;
    sihd::util::Handler<std::string_view, bool> test_output_handler(
        [&stdout_str, &stderr_str](std::string_view buf, bool is_stderr) {
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
    SshCommand cmd = session.make_command();
    cmd.output_handler = &test_output_handler;

    // Test async execution - command runs via proc::execute on server
    EXPECT_TRUE(cmd.execute_async("echo hello; echo world"));
    EXPECT_TRUE(cmd.wait(time::seconds(5)));

    EXPECT_EQ(stderr_str, "");
    EXPECT_EQ(stdout_str, "hello\nworld\n");
    EXPECT_EQ(cmd.exit_status(), 0);
}
} // namespace test
