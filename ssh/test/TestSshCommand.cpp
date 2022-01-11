#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/time.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshCommand.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    using namespace sihd::ssh;
    class TestSshCommand: public ::testing::Test
    {
        protected:
            TestSshCommand()
            {
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSshCommand()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "ssh", "sshcommand"});
    };

    TEST_F(TestSshCommand, test_sshcommand_simple)
    {
        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_key_auto();
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());

        std::string stdout_str;
        std::string stderr_str;
        sihd::util::Handler<char *, size_t, bool> test_output_handler(
        [&stdout_str, &stderr_str] (char *buf, size_t size, bool is_stderr)
        {
            buf[size] = 0;
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
        SshCommand cmd = session.make_command();
        cmd.output_handler = &test_output_handler;
        EXPECT_TRUE(cmd.execute("echo 'hello world'"));
        EXPECT_TRUE(cmd.wait());
        EXPECT_EQ(stderr_str, "");
        EXPECT_EQ(stdout_str, "hello world\n");
        EXPECT_EQ(cmd.exit_status(), 0);
    }

    TEST_F(TestSshCommand, test_sshcommand_async)
    {
        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_key_auto();
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());

        std::string stdout_str;
        std::string stderr_str;
        sihd::util::Handler<char *, size_t, bool> test_output_handler(
        [&stdout_str, &stderr_str] (char *buf, size_t size, bool is_stderr)
        {
            buf[size] = 0;
            if (is_stderr)
                stderr_str += buf;
            else
                stdout_str += buf;
        });
        SteadyClock clock;
        SshCommand cmd = session.make_command();
        cmd.output_handler = &test_output_handler;

        EXPECT_TRUE(cmd.execute_async("echo hello; sleep 1; echo world;"));
        
        time_t before = clock.now();
        EXPECT_TRUE(cmd.wait());
        time_t after = clock.now();
        EXPECT_GT(after - before, time::milli(900));

        EXPECT_EQ(stderr_str, "");
        EXPECT_EQ(stdout_str, "hello\nworld\n");
        EXPECT_EQ(cmd.exit_status(), 0);
    }
}