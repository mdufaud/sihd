#include <gtest/gtest.h>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::ssh;
class TestSshSession: public ::testing::Test
{
    protected:
        TestSshSession() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshSession() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshSession, test_sshsession_auth_key)
{
    std::string home = getenv("HOME");
    if (fs::is_file(fs::combine(home, ".ssh/id_rsa")) == false)
        GTEST_SKIP_("need ~/.ssh/id_rsa");
    std::string user = getenv("USER");
    SshSession session;

    GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
    session.set_verbosity(SSH_LOG_PROTOCOL);
    EXPECT_TRUE(session.connected());
    auto auth = session.auth_key_file(fs::combine(home, ".ssh/id_rsa"));
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.success());

    session.disconnect();

    GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
    EXPECT_TRUE(session.connected());
    auth = session.auth_key_auto();
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.success());
}

TEST_F(TestSshSession, test_sshsession_auth_interactive_keyboard)
{
    std::string home = getenv("HOME");
    if (fs::is_dir(fs::combine(home, ".ssh")) == false)
        GTEST_SKIP_("need ~/.ssh");
    if (term::is_interactive() == false)
        GTEST_SKIP_("need interactive keyboard");

    std::string host = "localhost";
    SIHD_LOG(info, "Initiating keyboard password connection to @{}", host);
    SIHD_LOG(info, "If you don't want to, you can leave user empty to skip the test");
    std::string user;
    std::cout << "User: ";
    fflush(stdout);
    if (LineReader::fast_read_stdin(user) == false || user.empty())
        GTEST_SKIP_("no user input");
    SIHD_LOG(info, "Connection to {}@{}", user, host);
    SshSession session;
    GTEST_ASSERT_EQ(session.fast_connect(user,
                                         host,
                                         22,
                                         SSH_LOG_PROTOCOL | SSH_LOG_DEBUG | SSH_LOG_PACKET | SSH_LOG_WARN),
                    true);
    session.set_verbosity(SSH_LOG_PROTOCOL);
    EXPECT_TRUE(session.connected());
    auto auth = session.auth_interactive_keyboard();
    SIHD_LOG(info, "Auth status: {}", auth.str());
}

TEST_F(TestSshSession, test_sshsession_connect)
{
    std::string home = getenv("HOME");
    if (fs::is_dir(fs::combine(home, ".ssh")) == false)
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
} // namespace test
