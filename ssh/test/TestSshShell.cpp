#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshShell.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::ssh;
    using namespace sihd::util;
    class TestSshShell: public ::testing::Test
    {
        protected:
            TestSshShell()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::FS::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "ssh",
                    "sshshell"
                });
                _cwd = sihd::util::OS::cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::FS::make_directories(_base_test_dir);
            }

            virtual ~TestSshShell()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestSshShell, test_sshshell_interactive)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");
        if (sihd::util::OS::is_run_by_valgrind())
            GTEST_SKIP_("no valgrind");

        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.auth_key_auto().success());

        SshShell shell = session.make_shell();
        ASSERT_TRUE(shell.open(true));
        SIHD_COUT("=========================================================");
        SIHD_LOG(debug, "Shell opened: type 'exit' to exit session");
        SIHD_COUT("=========================================================");
        ASSERT_TRUE(shell.read_loop());
    }
}