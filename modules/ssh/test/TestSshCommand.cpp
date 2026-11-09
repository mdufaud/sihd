#include <chrono>

#include <gtest/gtest.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemFunction.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::ssh;

namespace
{

// ends the command like a signal kill: an exit-signal and no exit-status
class SignaledFunction: public SshSubsystemFunction
{
    public:
        using SshSubsystemFunction::SshSubsystemFunction;

        int on_close() override
        {
            if (this->channel() != nullptr)
                (void)this->channel()->request_send_exit_signal("KILL", false, "killed", "");
            SshSubsystemFunction::on_close();
            return -1;
        }
};

} // namespace

class TestSshCommand: public ::testing::Test
{
    protected:
        TestSshCommand() { LoggerManager::stream(); }

        virtual ~TestSshCommand() { LoggerManager::clear_loggers(); }

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
    Handler<std::string_view, bool> test_output_handler(
        [&stdout_str, &stderr_str](std::string_view buf, bool is_stderr) {
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
    SshCommand cmd = session.make_command();
    cmd.output_handler = &test_output_handler;
    EXPECT_TRUE(cmd.execute("echo hello world").has_value());
    EXPECT_TRUE(cmd.wait(Duration(time::seconds(5))).has_value());
    EXPECT_EQ(stderr_str, "");
    EXPECT_EQ(stdout_str, build::is_windows ? "hello world\r\n" : "hello world\n");
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
    Handler<std::string_view, bool> test_output_handler(
        [&stdout_str, &stderr_str](std::string_view buf, bool is_stderr) {
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
    SshCommand cmd = session.make_command();
    cmd.output_handler = &test_output_handler;

    EXPECT_TRUE(cmd.execute_async("echo hello&& echo world").has_value());
    EXPECT_TRUE(cmd.wait(Duration(time::seconds(2))).has_value());

    EXPECT_EQ(stderr_str, "");
    EXPECT_EQ(stdout_str, build::is_windows ? "hello\r\nworld\r\n" : "hello\nworld\n");
    EXPECT_EQ(cmd.exit_status(), 0);
}

TEST_F(TestSshCommand, test_sshcommand_wait_returns_on_an_exit_signal)
{
    auto test_server = make_test_server("test-sshcommand-signal");
    ASSERT_NE(test_server, nullptr);
    test_server->handler.set_exec_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           std::string_view command,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *function = new SignaledFunction(command);
            function->set_callback([](const std::string &) {
                return SshSubsystemFunction::Result {.output = {}, .error_output = {}, .exit_code = 0};
            });
            return function;
        });
    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    SshCommand cmd = session.make_command();
    ASSERT_TRUE(cmd.execute_async("killed").has_value());

    const auto begin = std::chrono::steady_clock::now();
    const auto waited = cmd.wait(Duration(time::seconds(3)));
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    ASSERT_FALSE(waited.has_value());
    EXPECT_EQ(waited.error().message, "command terminated by signal 'KILL': killed");
    EXPECT_EQ(cmd.exit_signal_str(), "KILL");
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST_F(TestSshCommand, test_sshcommand_wait_returns_on_a_close_without_status)
{
    auto test_server = make_test_server("test-sshcommand-no-status");
    ASSERT_NE(test_server, nullptr);
    test_server->handler.set_exec_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           std::string_view command,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *function = new SshSubsystemFunction(command);
            // a negative exit code closes the channel without any exit status
            function->set_callback([](const std::string &) {
                return SshSubsystemFunction::Result {.output = {}, .error_output = {}, .exit_code = -1};
            });
            return function;
        });
    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    SshCommand cmd = session.make_command();
    ASSERT_TRUE(cmd.execute_async("no-status").has_value());

    const auto begin = std::chrono::steady_clock::now();
    const auto waited = cmd.wait(Duration(time::seconds(3)));
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    ASSERT_FALSE(waited.has_value());
    EXPECT_EQ(waited.error().code, ErrorCode::closed);
    EXPECT_EQ(waited.error().message, "could not wait for command: no exit status");
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}
} // namespace test
