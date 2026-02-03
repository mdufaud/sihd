#include <gtest/gtest.h>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
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
    ASSERT_TRUE(session.fast_connect("testuser", "127.0.0.1", port));
    EXPECT_TRUE(session.connected());

    auto auth = session.auth_key_file(CLIENT_KEY_PATH);
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.success());

    session.disconnect();
}
} // namespace test
