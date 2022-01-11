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

    TEST_F(TestSftp, test_sftp)
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
        EXPECT_TRUE(sftp.mkdir(test_dir + "/new_dir"));
        EXPECT_TRUE(Files::is_dir(test_dir + "/new_dir"));

        EXPECT_TRUE(sftp.send_file("test/resources/file.txt", test_dir + "/sent_file.txt"));
        EXPECT_TRUE(sftp.get_file(_cwd + "/test/resources/file.txt", test_dir + "/recv_file.txt"));
        EXPECT_TRUE(Files::is_file(test_dir + "/sent_file.txt"));
        EXPECT_TRUE(Files::is_file(test_dir + "/recv_file.txt"));
        EXPECT_EQ(Files::get_filesize(test_dir + "/sent_file.txt"), Files::get_filesize("test/resources/file.txt"));

        std::vector<std::string> list;
        EXPECT_TRUE(sftp.list_dir_filenames(test_dir, list));
        EXPECT_EQ(list.size(), 3u);
        EXPECT_TRUE(std::find(list.begin(), list.end(), "recv_file.txt") != list.end());
        EXPECT_TRUE(std::find(list.begin(), list.end(), "sent_file.txt") != list.end());
        EXPECT_TRUE(std::find(list.begin(), list.end(), "new_dir/") != list.end());
        for (const std::string & name: list)
        {
            LOG(debug, name);
        }

        std::vector<SftpAttribute> attrs;
        EXPECT_TRUE(sftp.list_dir(test_dir, attrs));
        EXPECT_EQ(list.size(), 3u);
        int nlink = 0;
        int nregular = 0;
        int ndir = 0;
        for (const SftpAttribute & attr: attrs)
        {
            if (attr.is_file())
            {
                EXPECT_EQ(attr.size(), Files::get_filesize("test/resources/file.txt"));
            }
            nlink += (int)attr.is_link();
            nregular += (int)attr.is_file();
            ndir += (int)attr.is_dir();
        }
        EXPECT_EQ(nlink, 0);
        EXPECT_EQ(nregular, 2);
        EXPECT_EQ(ndir, 1);
    }
}