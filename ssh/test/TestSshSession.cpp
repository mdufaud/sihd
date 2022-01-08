#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshChannel.hpp>

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::util;
    using namespace sihd::ssh;
    class TestSshSession: public ::testing::Test
    {
        protected:
            TestSshSession()
            {
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSshSession()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "ssh", "sshsession"});
    };

    TEST_F(TestSshSession, test_sshsession_auth_key)
    {
        std::string home = getenv("HOME");
        if (Files::is_file(Files::combine(home, ".ssh/id_rsa")) == false)
            GTEST_SKIP_("need ~/.ssh/id_rsa");
        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        session.set_verbosity(SSH_LOG_PROTOCOL);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_key_file(Files::combine(home, ".ssh/id_rsa"));
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());

        session.disconnect();

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        auth = session.auth_key_auto();
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());
    }

    TEST_F(TestSshSession, test_sshsession_auth_interactive_keyboard)
    {
        std::string home = getenv("HOME");
        if (Files::is_dir(Files::combine(home, ".ssh")) == false)
            GTEST_SKIP_("need ~/.ssh");
        if (Term::is_interactive() == false)
            GTEST_SKIP_("need interactive keyboard");

        std::string user;
        std::cout << "User: ";
        fflush(stdout);
        if (LineReader::fast_read_line(user, stdin) == false)
            GTEST_SKIP_("no user input");


        SshSession session;
        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        session.set_verbosity(SSH_LOG_PROTOCOL);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_interactive_keyboard();
        LOG(info, "Auth status: " << auth.to_string());
    }

    TEST_F(TestSshSession, test_sshsession_connect)
    {
        std::string home = getenv("HOME");
        if (Files::is_dir(Files::combine(home, ".ssh")) == false)
            GTEST_SKIP_("need ~/.ssh");
        std::string user = getenv("USER");
        SshSession session;

        EXPECT_TRUE(session.new_session());
        session.set_verbosity(SSH_LOG_PROTOCOL);
        EXPECT_TRUE(session.set_user(user));
        EXPECT_TRUE(session.set_host("localhost"));
        EXPECT_TRUE(session.set_port(22));
        GTEST_ASSERT_EQ(session.connect(), true);
        EXPECT_TRUE(session.check_hostkey());
        EXPECT_TRUE(session.connected());
    }
}