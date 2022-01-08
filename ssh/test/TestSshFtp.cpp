#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/ssh/SshFtp.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::ssh;
    using namespace sihd::util;
    class TestSshFtp: public ::testing::Test
    {
        protected:
            TestSshFtp()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "ssh",
                    "SshFtp"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSshFtp()
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

    TEST_F(TestSshFtp, test_sshftp)
    {
        EXPECT_EQ(true, true);
    }

    TEST_F(TestSshFtp, test_sshftp_interactive)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");
    }

    TEST_F(TestSshFtp, test_sshftp_file)
    {
        std::string test_dir = sihd::util::Files::combine(_base_test_dir, "file");
        sihd::util::Files::remove_directories(test_dir);
        sihd::util::Files::make_directories(test_dir);
    }
}