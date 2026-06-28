#include <chrono>

#include <gtest/gtest.h>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/util/term.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::ssh;

class TestSshSession: public ::testing::Test
{
    protected:
        TestSshSession() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshSession() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshSession, test_sshsession_connect)
{
    auto test_server = make_test_server("test-sshsession-connect");
    ASSERT_NE(test_server, nullptr);

    SshSession session;
    EXPECT_TRUE(session.new_session());
    EXPECT_TRUE(session.set_user("testuser"));
    EXPECT_TRUE(session.set_host("127.0.0.1"));
    EXPECT_TRUE(session.set_port(test_server->port));
    GTEST_ASSERT_EQ(session.connect(), true);
    EXPECT_TRUE(session.connected());

    auto auth = session.auth_password("testpass");
    EXPECT_TRUE(auth.success());

    session.disconnect();
}

TEST_F(TestSshSession, test_sshsession_auth_key)
{
    // Read public key to add to allowed keys
    SshKey client_pubkey;
    ASSERT_TRUE(client_pubkey.import_pubkey_file(CLIENT_PUBKEY_PATH));
    std::string pubkey_base64 = client_pubkey.base64();
    ASSERT_FALSE(pubkey_base64.empty());

    auto test_server = make_test_server("test-sshsession-auth-key");
    ASSERT_NE(test_server, nullptr);

    test_server->handler.add_allowed_pubkey("testuser", pubkey_base64);

    int port = test_server->port;
    ASSERT_GT(port, 0);

    // Test auth_key_file
    SshSession session;
    ASSERT_TRUE(session.fast_connect({.user = "testuser", .host = "127.0.0.1", .port = port, .process_config = false}));
    EXPECT_TRUE(session.connected());

    auto auth = session.auth_key_file(CLIENT_KEY_PATH);
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.success());

    session.disconnect();
}

// A connect to an unresponsive host must not hang: the default timeout (set by
// new_session) bounds the attempt. 192.0.2.1 is RFC 5737 TEST-NET-1, guaranteed
// non-routable, so the SYN is dropped or instantly rejected; either way connect
// returns false quickly instead of blocking forever.
TEST_F(TestSshSession, test_sshsession_connect_timeout)
{
    SshSession session;
    ASSERT_TRUE(session.new_session());
    ASSERT_TRUE(session.set_user("testuser"));

    const auto start = std::chrono::steady_clock::now();
    const bool connected = session.fast_connect(
        {.user = "testuser", .host = "192.0.2.1", .port = 22, .process_config = false, .timeout_sec = 1});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(connected);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 5);
}

// Regression: process_config must control whether libssh parses ssh_config.
// A broken ProxyCommand in the config dir breaks the connection only when
// config processing is enabled; disabling it must ignore the config entirely.
TEST_F(TestSshSession, test_sshsession_process_config_ignores_proxy)
{
    auto test_server = make_test_server("test-sshsession-process-config");
    ASSERT_NE(test_server, nullptr);

    TmpDir ssh_dir;
    ASSERT_TRUE(static_cast<bool>(ssh_dir));
    std::string config_path = fs::combine(ssh_dir.path(), "config");
    ASSERT_TRUE(fs::write(config_path, "Host *\n    ProxyCommand /bin/false\n"));

    // Config processed (libssh default): ProxyCommand /bin/false breaks connect
    {
        SshSession session;
        ASSERT_TRUE(session.new_session());
        ASSERT_TRUE(session.set_ssh_dir(ssh_dir.path()));
        ASSERT_TRUE(session.set_user("testuser"));
        ASSERT_TRUE(session.set_host("127.0.0.1"));
        ASSERT_TRUE(session.set_port(test_server->port));
        EXPECT_FALSE(session.connect());
    }

    // Config processing disabled: ProxyCommand ignored, connect succeeds
    {
        SshSession session;
        ASSERT_TRUE(session.new_session());
        ASSERT_TRUE(session.set_ssh_dir(ssh_dir.path()));
        ASSERT_TRUE(session.set_user("testuser"));
        ASSERT_TRUE(session.set_host("127.0.0.1"));
        ASSERT_TRUE(session.set_port(test_server->port));
        ASSERT_TRUE(session.set_process_config(false));
        EXPECT_TRUE(session.connect());
    }
}
} // namespace test
