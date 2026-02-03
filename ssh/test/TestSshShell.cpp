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
        TestSshShell() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshShell() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshShell, test_sshshell_interactive)
{
    // This test requires:
    // 1. An interactive terminal
    // 2. A running SSH server on localhost:22
    // 3. Valid SSH key authentication
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");
    if (sihd::util::os::is_run_by_valgrind())
        GTEST_SKIP_("no valgrind");

    std::string user = getenv("USER");
    SshSession session;

    if (session.fast_connect(user, "localhost", 22) == false)
        GTEST_SKIP_("no SSH server on localhost:22");
    if (session.auth_key_auto().success() == false)
        GTEST_SKIP_("SSH key authentication failed");
    session.set_verbosity(SSH_LOG_PROTOCOL);

    SshShell shell = session.make_shell();
    ASSERT_TRUE(shell.open(true));
    SIHD_COUT("=========================================================\n");
    SIHD_LOG(debug, "Shell opened: type 'exit' to exit session");
    SIHD_COUT("=========================================================\n");
    ASSERT_TRUE(shell.read_loop());
}
} // namespace test
