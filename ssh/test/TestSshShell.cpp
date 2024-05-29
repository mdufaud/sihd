#include <gtest/gtest.h>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshShell.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;
class TestSshShell: public ::testing::Test
{
    protected:
        TestSshShell() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSshShell() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshShell, test_sshshell_interactive)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");
    if (sihd::util::os::is_run_by_valgrind())
        GTEST_SKIP_("no valgrind");

    std::string user = getenv("USER");
    SshSession session;

    GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
    EXPECT_TRUE(session.auth_key_auto().success());
    session.set_verbosity(SSH_LOG_PROTOCOL);

    SshShell shell = session.make_shell();
    ASSERT_TRUE(shell.open(true));
    SIHD_COUT("=========================================================\n");
    SIHD_LOG(debug, "Shell opened: type 'exit' to exit session");
    SIHD_COUT("=========================================================\n");
    ASSERT_TRUE(shell.read_loop());
}
} // namespace test
