#include <filesystem>

#include <gtest/gtest.h>
#include <libssh/sftp.h>

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

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

        void open_jail(std::string_view name)
        {
            _jail = fs::combine(_base.path(), "jail");
            ASSERT_TRUE(fs::make_directories(_jail));
            _server = make_test_server_with_sftp(name, _jail);
            ASSERT_NE(_server, nullptr);
            ASSERT_TRUE(connect_to_test_server(*_server, _session));
            _client = std::make_unique<SftpPacketClient>(_session);
            ASSERT_TRUE(_client->open());
        }

        int write_at(const std::string & handle, uint64_t offset, std::string_view data)
        {
            auto reply = _client->request(
                SSH_FXP_WRITE,
                SftpPacketClient::str(handle) + SftpPacketClient::u64(offset) + SftpPacketClient::str(data));
            return reply.has_value() ? reply->status() : -1;
        }

        int close_handle(const std::string & handle)
        {
            auto reply = _client->request(SSH_FXP_CLOSE, SftpPacketClient::str(handle));
            return reply.has_value() ? reply->status() : -1;
        }

        std::string client_realpath(std::string_view path)
        {
            constexpr size_t name_count_size = sizeof(uint32_t);
            auto reply = _client->request_path(SSH_FXP_REALPATH, path);
            if (!reply.has_value() || reply->type != SSH_FXP_NAME)
                return "<refused>";
            return reply->string_at(name_count_size);
        }

        TmpDir _base;
        std::string _jail;
        std::unique_ptr<SshServerHelper> _server;
        SshSession _session;
        std::unique_ptr<SftpPacketClient> _client;
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

    auto listed = sftp.list_dir_filenames(test_dir);
    ASSERT_TRUE(listed.has_value());
    const std::vector<std::string> & list = *listed;
    EXPECT_EQ(list.size(), 3u);
    EXPECT_TRUE(std::find(list.begin(), list.end(), "recv_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "sent_file.txt") != list.end());
    EXPECT_TRUE(std::find(list.begin(), list.end(), "new_dir/") != list.end());
    for (const std::string & name : list)
    {
        SIHD_LOG(debug, "{}", name);
    }

    auto attrs = sftp.list_dir(test_dir);
    ASSERT_TRUE(attrs.has_value());
    EXPECT_EQ(attrs->size(), 3u);

    int nlink = 0;
    int nregular = 0;
    int ndir = 0;

    for (const SftpAttribute & attr : *attrs)
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

    auto attrs = sftp.list_dir(tmp_dir.path());
    ASSERT_TRUE(attrs.has_value());
    ASSERT_EQ(attrs->size(), 1u);
    EXPECT_EQ((*attrs)[0].size(), static_cast<uint64_t>(large_size));
}

TEST_F(TestSftp, test_sftp_readlink)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "the sftp server refuses symbolic links on windows";

    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);

    const std::string target = fs::combine(tmp_dir.path(), "target.txt");
    const std::string link = fs::combine(tmp_dir.path(), "link.txt");
    ASSERT_TRUE(fs::write(target, "hello"));
    std::filesystem::create_symlink(target, link);

    auto test_server = make_test_server_with_sftp("test-sftp-readlink", "/");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    auto read_target = sftp.readlink(link);
    ASSERT_TRUE(read_target.has_value()) << read_target.error().message;
    EXPECT_EQ(*read_target, target);

    EXPECT_FALSE(sftp.readlink(target).has_value());
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

    EXPECT_FALSE(sftp.get_file("../secret.txt", leaked).has_value());
    EXPECT_FALSE(fs::is_file(leaked));

    std::string got = fs::combine(out.path(), "got.txt");
    EXPECT_TRUE(sftp.get_file("ok.txt", got).has_value());
    EXPECT_TRUE(fs::is_file(got));

    EXPECT_FALSE(sftp.send_file(fs::combine(jail, "ok.txt"), "../escape.txt").has_value());
    EXPECT_FALSE(fs::is_file(fs::combine(base.path(), "escape.txt")));
}

TEST_F(TestSftp, test_sftp_sessions_in_a_row_on_one_connection)
{
    TmpDir base;
    ASSERT_TRUE(fs::write(fs::combine(base.path(), "ok.txt"), "hello"));
    auto test_server = make_test_server_with_sftp("test-sftp-in-a-row", base.path());
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    // the next channel may reuse the address of the sftp channel the server just freed
    for (int i = 0; i < 5; ++i)
    {
        Sftp sftp = session.make_sftp();
        ASSERT_TRUE(sftp.open().has_value()) << "sftp session " << i;
        EXPECT_EQ(sftp.list_dir_filenames("/").value_or(std::vector<std::string> {}),
                  std::vector<std::string> {"ok.txt"})
            << "sftp session " << i;
    }
}

TEST_F(TestSftp, test_sftp_get_missing_file_keeps_local_file)
{
    TmpDir base;
    auto test_server = make_test_server_with_sftp("test-sftp-get-missing", base.path());
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    ASSERT_TRUE(connect_to_test_server(*test_server, session));

    Sftp sftp = session.make_sftp();
    ASSERT_TRUE(sftp.open().has_value());

    TmpDir out;
    const std::string local = fs::combine(out.path(), "keep.txt");
    ASSERT_TRUE(fs::write(local, "precious"));

    EXPECT_FALSE(sftp.get_file("missing.txt", local).has_value());

    EXPECT_EQ(fs::read_all(local).value_or(""), "precious");
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

TEST_F(TestSftp, test_sftp_read_length_capped)
{
    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-read-cap"));
    ASSERT_TRUE(fs::write(fs::combine(_jail, "big.bin"), std::string(1024 * 1024, 'x')));

    const std::string handle = _client->open_file("/big.bin", SSH_FXF_READ);
    ASSERT_FALSE(handle.empty());

    auto reply = _client->request(
        SSH_FXP_READ,
        SftpPacketClient::str(handle) + SftpPacketClient::u64(0) + SftpPacketClient::u32(1024 * 1024));
    ASSERT_TRUE(reply.has_value());
    ASSERT_EQ(reply->type, SSH_FXP_DATA);
    EXPECT_EQ(reply->string_at(0).size(), 255u * 1024);
}

TEST_F(TestSftp, test_sftp_readdir_pages_a_big_directory)
{
    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-readdir-pages"));
    const std::string big_dir = fs::combine(_jail, "big");
    ASSERT_TRUE(fs::make_directories(big_dir));
    constexpr uint32_t file_count = 3000;
    for (uint32_t i = 0; i < file_count; ++i)
        ASSERT_TRUE(fs::write(fs::combine(big_dir, "file_with_a_rather_long_name_" + std::to_string(i)), ""));
    auto opened = _client->request_path(SSH_FXP_OPENDIR, "/big");
    ASSERT_TRUE(opened.has_value());
    ASSERT_EQ(opened->type, SSH_FXP_HANDLE);
    const std::string handle = opened->string_at(0);

    uint32_t listed = 0;
    size_t biggest_reply = 0;
    auto reply = _client->request(SSH_FXP_READDIR, SftpPacketClient::str(handle));
    while (reply.has_value() && reply->type == SSH_FXP_NAME)
    {
        listed += SftpPacketClient::read_u32(reply->body, 0);
        biggest_reply = std::max(biggest_reply, reply->body.size());
        reply = _client->request(SSH_FXP_READDIR, SftpPacketClient::str(handle));
    }

    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status(), SSH_FX_EOF);
    EXPECT_EQ(listed, file_count);
    // an OpenSSH client refuses a packet over 256 KiB
    EXPECT_LT(biggest_reply, 256u * 1024);
}

TEST_F(TestSftp, test_sftp_open_flags_keep_data)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "the sftp server needs a jail root";

    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-open-flags"));
    const std::string file = fs::combine(_jail, "data.txt");
    ASSERT_TRUE(fs::write(file, "0123456789"));

    std::string handle = _client->open_file("/data.txt", SSH_FXF_WRITE | SSH_FXF_CREAT);
    ASSERT_FALSE(handle.empty());
    EXPECT_EQ(write_at(handle, 2, "ab"), SSH_FX_OK);
    EXPECT_EQ(close_handle(handle), SSH_FX_OK);
    EXPECT_EQ(fs::read_all(file).value(), "01ab456789");

    auto excl = _client->request(SSH_FXP_OPEN,
                                 SftpPacketClient::str("/data.txt")
                                     + SftpPacketClient::u32(SSH_FXF_WRITE | SSH_FXF_CREAT | SSH_FXF_EXCL)
                                     + SftpPacketClient::u32(0));
    ASSERT_TRUE(excl.has_value());
    EXPECT_EQ(excl->status(), SSH_FX_FILE_ALREADY_EXISTS);
    EXPECT_EQ(fs::read_all(file).value(), "01ab456789");

    handle = _client->open_file("/data.txt", SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_CREAT);
    ASSERT_FALSE(handle.empty());
    EXPECT_EQ(write_at(handle, 0, "cd"), SSH_FX_OK);
    EXPECT_EQ(close_handle(handle), SSH_FX_OK);
    EXPECT_EQ(fs::read_all(file).value(), "cdab456789");

    handle = _client->open_file("/data.txt", SSH_FXF_WRITE | SSH_FXF_APPEND);
    ASSERT_FALSE(handle.empty());
    EXPECT_EQ(write_at(handle, 0, "!"), SSH_FX_OK);
    EXPECT_EQ(close_handle(handle), SSH_FX_OK);
    EXPECT_EQ(fs::read_all(file).value(), "cdab456789!");

    handle = _client->open_file("/data.txt", SSH_FXF_WRITE | SSH_FXF_CREAT | SSH_FXF_TRUNC);
    ASSERT_FALSE(handle.empty());
    EXPECT_EQ(write_at(handle, 0, "z"), SSH_FX_OK);
    EXPECT_EQ(close_handle(handle), SSH_FX_OK);
    EXPECT_EQ(fs::read_all(file).value(), "z");
}

TEST_F(TestSftp, test_sftp_fsetstat_targets_handle)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "permission bits are not posix on windows";

    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-fsetstat"));
    const std::string file = fs::combine(_jail, "a.txt");
    ASSERT_TRUE(fs::write(file, "hello"));
    ASSERT_TRUE(fs::permission_set(file, 0644));
    const unsigned int jail_mode = fs::permission_get(_jail);

    const std::string handle = _client->open_file("/a.txt", SSH_FXF_READ | SSH_FXF_WRITE);
    ASSERT_FALSE(handle.empty());
    auto reply = _client->request(SSH_FXP_FSETSTAT,
                                  SftpPacketClient::str(handle) + SftpPacketClient::u32(SSH_FILEXFER_ATTR_PERMISSIONS)
                                      + SftpPacketClient::u32(0600));
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status(), SSH_FX_OK);

    EXPECT_EQ(fs::permission_get(file), 0600u);
    EXPECT_EQ(fs::permission_get(_jail), jail_mode);
}

TEST_F(TestSftp, test_sftp_realpath_stays_in_jail)
{
    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-realpath"));
    ASSERT_TRUE(fs::make_directories(fs::combine(_jail, "sub")));

    EXPECT_EQ(client_realpath("."), "/");
    EXPECT_EQ(client_realpath("/"), "/");
    EXPECT_EQ(client_realpath("/sub/.."), "/");
    EXPECT_EQ(client_realpath("sub"), "/sub");
    EXPECT_EQ(client_realpath("/sub/./"), "/sub");
}

TEST_F(TestSftp, test_sftp_lstat_jail_root)
{
    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-lstat-root"));

    auto root = _client->request_path(SSH_FXP_LSTAT, "/");
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->type, SSH_FXP_ATTRS);

    auto dot = _client->request_path(SSH_FXP_LSTAT, ".");
    ASSERT_TRUE(dot.has_value());
    EXPECT_EQ(dot->type, SSH_FXP_ATTRS);
}

TEST_F(TestSftp, test_sftp_partial_packet_leaves_the_server_serving)
{
    ASSERT_NO_FATAL_FAILURE(open_jail("test-sftp-partial-packet"));
    // a request length announced, its bytes never sent
    ASSERT_TRUE(_client->send_raw(SftpPacketClient::u32(64)));

    SshSession other;
    Stopwatch watch;
    const bool connected = connect_to_test_server(*_server, other);
    const Duration connect_time = watch.time();

    EXPECT_TRUE(connected);
    EXPECT_LT(connect_time, Duration(time::sec(2)));
}
} // namespace test
