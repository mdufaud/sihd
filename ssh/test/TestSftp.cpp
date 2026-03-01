#include <gtest/gtest.h>
#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;
using namespace sihd::sys;

class TestSftp: public ::testing::Test
{
    protected:
        TestSftp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSftp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSftp, test_sftp)
{
    TmpDir tmp_dir;

    std::string test_dir = sihd::sys::fs::combine(tmp_dir.path(), "mkdir");
    sihd::sys::fs::remove_directories(test_dir);
    sihd::sys::fs::make_directories(test_dir);

    // Start test server with SFTP enabled, using tmp_dir as root
    auto test_server = make_test_server_with_sftp("test-sftp", "/");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    EXPECT_TRUE(session.connected());

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open());
    EXPECT_TRUE(sftp.mkdir(test_dir + "/new_dir"));
    EXPECT_TRUE(fs::is_dir(test_dir + "/new_dir"));

    EXPECT_TRUE(sftp.send_file("test/resources/file.txt", test_dir + "/sent_file.txt"));
    EXPECT_TRUE(sftp.get_file(fs::cwd() + "/test/resources/file.txt", test_dir + "/recv_file.txt"));
    EXPECT_TRUE(fs::is_file(test_dir + "/sent_file.txt"));
    EXPECT_TRUE(fs::is_file(test_dir + "/recv_file.txt"));
    EXPECT_EQ(fs::file_size(test_dir + "/sent_file.txt"), fs::file_size("test/resources/file.txt"));

    std::vector<std::string> list;
    EXPECT_TRUE(sftp.list_dir_filenames(test_dir, list));
    EXPECT_EQ(list.size(), 3u);
    EXPECT_TRUE(std::find(list.begin(), list.end(), "recv_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "sent_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "new_dir/") != list.end());
    for (const std::string & name : list)
    {
        SIHD_LOG(debug, "{}", name);
    }

    std::vector<SftpAttribute> attrs;
    EXPECT_TRUE(sftp.list_dir(test_dir, attrs));
    EXPECT_EQ(attrs.size(), 3u);
    int nlink = 0;
    int nregular = 0;
    int ndir = 0;
    for (const SftpAttribute & attr : attrs)
    {
        if (attr.is_file())
        {
            EXPECT_EQ(attr.size(), fs::file_size("test/resources/file.txt"));
        }
        nlink += (int)attr.is_link();
        nregular += (int)attr.is_file();
        ndir += (int)attr.is_dir();
    }
    EXPECT_EQ(nlink, 0);
    EXPECT_EQ(nregular, 2);
    EXPECT_EQ(ndir, 1);
}
} // namespace test
