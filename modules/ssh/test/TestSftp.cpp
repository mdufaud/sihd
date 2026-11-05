#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

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
        TestSftp() { LoggerManager::stream(); }

        virtual ~TestSftp() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSftp, test_sftp)
{
    TmpDir tmp_dir;

    std::string test_dir = sihd::sys::fs::combine(tmp_dir.path(), "mkdir");
    SIHD_UNEXPECTED_LOG(sihd::sys::fs::remove_directories(test_dir));
    SIHD_UNEXPECTED_LOG(sihd::sys::fs::make_directories(test_dir));

    // windows has no single root, the absolute test paths start with a drive: no jail
    auto test_server = make_test_server_with_sftp("test-sftp", build::is_windows ? "" : "/");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));
    EXPECT_TRUE(session.connected());

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());
    EXPECT_TRUE(sftp.mkdir(test_dir + "/new_dir").has_value());
    EXPECT_TRUE(fs::is_dir(test_dir + "/new_dir"));

    EXPECT_TRUE(sftp.send_file("test/resources/file.txt", test_dir + "/sent_file.txt").has_value());
    EXPECT_TRUE(sftp.get_file(fs::cwd() + "/test/resources/file.txt", test_dir + "/recv_file.txt").has_value());
    EXPECT_TRUE(fs::is_file(test_dir + "/sent_file.txt"));
    EXPECT_TRUE(fs::is_file(test_dir + "/recv_file.txt"));
    EXPECT_EQ(fs::file_size(test_dir + "/sent_file.txt").value(), fs::file_size("test/resources/file.txt").value());

    std::vector<std::string> list;
    EXPECT_TRUE(sftp.list_dir_filenames(test_dir, list).has_value());
    EXPECT_EQ(list.size(), 3u);
    EXPECT_TRUE(std::find(list.begin(), list.end(), "recv_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "sent_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "new_dir/") != list.end());
    for (const std::string & name : list)
    {
        SIHD_LOG(debug, "{}", name);
    }

    std::vector<SftpAttribute> attrs;
    EXPECT_TRUE(sftp.list_dir(test_dir, attrs).has_value());
    EXPECT_EQ(attrs.size(), 3u);

    int nlink = 0;
    int nregular = 0;
    int ndir = 0;

    for (const SftpAttribute & attr : attrs)
    {
        if (attr.is_file())
        {
            EXPECT_EQ(attr.size(), fs::file_size("test/resources/file.txt").value());
        }
        nlink += (int)attr.is_link();
        nregular += (int)attr.is_file();
        ndir += (int)attr.is_dir();
    }
    EXPECT_EQ(nlink, 0);
    EXPECT_EQ(nregular, 2);
    EXPECT_EQ(ndir, 1);
}

TEST_F(TestSftp, test_sftp_large_file_attribute)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);

    const std::string large_file = fs::combine(tmp_dir.path(), "large.bin");
    const int64_t large_size = 5LL * 1024 * 1024 * 1024;
    ASSERT_TRUE(fs::write_binary(large_file, "").has_value());
    ASSERT_TRUE(fs::truncate(large_file, large_size).has_value());

    auto test_server = make_test_server_with_sftp("test-sftp-large", build::is_windows ? "" : "/");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    std::vector<SftpAttribute> attrs;
    ASSERT_TRUE(sftp.list_dir(tmp_dir.path(), attrs).has_value());
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].size(), static_cast<uint64_t>(large_size));
}

TEST_F(TestSftp, test_sftp_jail_blocks_traversal)
{
    TmpDir base;
    std::string jail = fs::combine(base.path(), "jail");
    ASSERT_TRUE(fs::make_directories(jail));

    // Secret sits OUTSIDE the jail (in its parent)
    std::string secret = fs::combine(base.path(), "secret.txt");
    ASSERT_TRUE(fs::write(secret, "TOPSECRET"));
    ASSERT_TRUE(fs::write(fs::combine(jail, "ok.txt"), "hello"));

    auto test_server = make_test_server_with_sftp("test-sftp-jail", jail);
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    TmpDir out;
    std::string leaked = fs::combine(out.path(), "leaked.txt");

    // Traversal read must fail without leaking the secret; get_file opens the local sink before the
    // remote file so an empty local file may remain - the guarantee is no secret bytes leak
    EXPECT_FALSE(sftp.get_file("../secret.txt", leaked).has_value());
    if (fs::is_file(leaked))
    {
        EXPECT_EQ(fs::file_size(leaked).value(), 0u);
    }

    std::string got = fs::combine(out.path(), "got.txt");
    EXPECT_TRUE(sftp.get_file("ok.txt", got).has_value());
    EXPECT_TRUE(fs::is_file(got));

    EXPECT_FALSE(sftp.send_file(fs::combine(jail, "ok.txt"), "../escape.txt").has_value());
    EXPECT_FALSE(fs::is_file(fs::combine(base.path(), "escape.txt")));
}

TEST_F(TestSftp, test_sftp_jail_escaping_link_blocks_writes)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "the sftp server refuses symbolic links on windows";

    TmpDir base;
    const std::string jail = fs::combine(base.path(), "jail");
    const std::string outside = fs::combine(base.path(), "outside");
    ASSERT_TRUE(fs::make_directories(jail));
    ASSERT_TRUE(fs::make_directories(outside));
    ASSERT_TRUE(fs::write(fs::combine(jail, "ok.txt"), "hello"));
    std::filesystem::create_directory_symlink(outside, fs::combine(jail, "out"));

    auto test_server = make_test_server_with_sftp("test-sftp-escaping-link", jail);
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    EXPECT_FALSE(sftp.mkdir("/out/created").has_value());
    EXPECT_FALSE(sftp.send_file(fs::combine(jail, "ok.txt"), "/out/sent.txt").has_value());

    EXPECT_FALSE(fs::exists(fs::combine(outside, "created")));
    EXPECT_FALSE(fs::exists(fs::combine(outside, "sent.txt")));
}

TEST_F(TestSftp, test_sftp_jail_escape_never_targets_root)
{
    TmpDir base;
    const std::string jail = fs::combine(base.path(), "jail");
    ASSERT_TRUE(fs::make_directories(jail));
    const unsigned int jail_mode = fs::permission_get(jail);

    auto test_server = make_test_server_with_sftp("test-sftp-escape-root", jail);
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    EXPECT_FALSE(sftp.chmod("/../elsewhere", 0500).has_value());
    EXPECT_FALSE(sftp.rmdir("/../elsewhere").has_value());
    EXPECT_FALSE(sftp.rmdir("/").has_value());

    EXPECT_TRUE(fs::is_dir(jail));
    EXPECT_EQ(fs::permission_get(jail), jail_mode);
}

TEST_F(TestSftp, test_sftp_link_operations_keep_target)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "the sftp server refuses symbolic links on windows";

    TmpDir jail;
    const std::string target = fs::combine(jail.path(), "target.txt");
    ASSERT_TRUE(fs::write(target, "hello"));
    std::filesystem::create_symlink(target, fs::combine(jail.path(), "removed.txt"));
    std::filesystem::create_symlink(target, fs::combine(jail.path(), "renamed.txt"));

    auto test_server = make_test_server_with_sftp("test-sftp-link-ops", jail.path());
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    EXPECT_TRUE(sftp.rm("/removed.txt").has_value());
    EXPECT_TRUE(sftp.rename("/renamed.txt", "/moved.txt").has_value());

    EXPECT_TRUE(fs::is_file(target));
    EXPECT_FALSE(std::filesystem::is_symlink(std::filesystem::symlink_status(fs::combine(jail.path(), "removed.txt"))));
    EXPECT_TRUE(std::filesystem::is_symlink(fs::combine(jail.path(), "moved.txt")));
}
} // namespace test
