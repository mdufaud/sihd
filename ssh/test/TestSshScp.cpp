#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshScp.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::ssh;
    using namespace sihd::util;
    class TestSshScp: public ::testing::Test
    {
        protected:
            TestSshScp()
            {
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestSshScp()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "ssh", "sshscp"});
    };

    TEST_F(TestSshScp, test_sshscp_push)
    {
        std::string test_dir = Files::combine(_base_test_dir, "push");
        Files::remove_directories(test_dir);
        Files::make_directories(test_dir);

        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_key_auto();
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());

        SshScp scp = session.make_scp();

        EXPECT_TRUE(scp.open_remote(test_dir));
        EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file.txt"));
        EXPECT_TRUE(scp.push_dir("pushed_dir"));
        EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file_in_dir.txt"));
        EXPECT_TRUE(scp.leave_dir());
        EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file_not_in_dir.txt"));

        scp.close();
        EXPECT_FALSE(scp.push_file("test/resources/file.txt", "pushed_file2.txt"));
        EXPECT_FALSE(scp.push_dir("pushed_dir2"));
        EXPECT_FALSE(scp.leave_dir());

        EXPECT_TRUE(Files::is_file(Files::combine(test_dir, "pushed_file.txt")));
        EXPECT_TRUE(Files::is_file(Files::combine(test_dir, "pushed_dir/pushed_file_in_dir.txt")));
        EXPECT_TRUE(Files::is_file(Files::combine(test_dir, "pushed_file_not_in_dir.txt")));

        EXPECT_EQ(Files::get_filesize(test_dir + "/pushed_file.txt"), Files::get_filesize("test/resources/file.txt"));

        EXPECT_FALSE(Files::is_file(Files::combine(test_dir, "pushed_file2.txt")));
        EXPECT_FALSE(Files::is_dir(Files::combine(test_dir, "pushed_dir2")));
    }

    TEST_F(TestSshScp, test_sshscp_pull)
    {
        std::string test_dir = Files::combine(_base_test_dir, "pull");
        Files::remove_directories(test_dir);
        Files::make_directories(test_dir);

        std::string user = getenv("USER");
        SshSession session;

        GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
        EXPECT_TRUE(session.connected());
        auto auth = session.auth_key_auto();
        LOG(info, "Auth status: " << auth.to_string());
        EXPECT_TRUE(auth.success());

        SshScp scp = session.make_scp();

        std::string pull_from = Files::combine(OS::get_cwd(), "test/resources/file.txt");
        std::string pull_to = Files::combine(test_dir, "pulled_file.txt");
        EXPECT_TRUE(scp.pull_file(pull_from, pull_to));
    }
}