#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/Sftp.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::ssh;
    using namespace sihd::util;
    class TestSftp: public ::testing::Test
    {
        protected:
            TestSftp()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "ssh",
                    "sftp"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSftp()
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

    TEST_F(TestSftp, test_sftp_mkdir)
    {
        std::string test_dir = sihd::util::Files::combine(_base_test_dir, "mkdir");
        sihd::util::Files::remove_directories(test_dir);
        sihd::util::Files::make_directories(test_dir);

        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        EXPECT_TRUE(session.auth_key_auto().success());

        Sftp sftp = session.make_sftp();
        EXPECT_TRUE(sftp.open());
        EXPECT_TRUE(sftp.mkdir(Files::combine(test_dir, "new_dir")));

        EXPECT_TRUE(Files::is_dir(Files::combine(test_dir, "new_dir")));
    }
}