#include <gtest/gtest.h>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/time.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;

class TestSshServer: public ::testing::Test
{
    protected:
        TestSshServer() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSshServer() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSshServer, test_password_auth_failure)
{
    SshServer server("test-ssh-server");
    BasicSshServerHandler handler;

    handler.add_allowed_user("testuser", "testpass");

    ASSERT_TRUE(server.set_port(0));
    ASSERT_TRUE(server.set_rsa_key(HOST_KEY_PATH));
    server.set_server_handler(&handler);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ssh-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    int port = server.get_port();
    ASSERT_GT(port, 0);

    // Client connects
    SshSession client;
    ASSERT_TRUE(client.fast_connect("testuser", "127.0.0.1", port));
    EXPECT_TRUE(client.connected());

    auto auth = client.auth_password("wrongpass");
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.denied());

    client.disconnect();

    server.stop();
    worker.stop_worker();

    EXPECT_EQ(handler.counters().auth_password_success, 0u);
    EXPECT_GE(handler.counters().auth_password_fail, 1u);
}

TEST_F(TestSshServer, test_pubkey_auth_success)
{
    // Read public key to add to allowed keys
    SshKey client_pubkey;
    ASSERT_TRUE(client_pubkey.import_pubkey_file(CLIENT_PUBKEY_PATH));
    std::string pubkey_base64 = client_pubkey.base64();
    ASSERT_FALSE(pubkey_base64.empty());

    SshServer server("test-ssh-server");
    BasicSshServerHandler handler;

    handler.add_allowed_pubkey("testuser", pubkey_base64);

    ASSERT_TRUE(server.set_port(0));
    ASSERT_TRUE(server.set_rsa_key(HOST_KEY_PATH));
    server.set_server_handler(&handler);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ssh-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    int port = server.get_port();
    ASSERT_GT(port, 0);

    // Client connects with pubkey auth
    SshSession client;
    ASSERT_TRUE(client.fast_connect("testuser", "127.0.0.1", port));
    EXPECT_TRUE(client.connected());

    auto auth = client.auth_key_file(CLIENT_KEY_PATH);
    SIHD_LOG(info, "Auth status: {}", auth.str());
    EXPECT_TRUE(auth.success());

    client.disconnect();

    server.stop();
    worker.stop_worker();

    EXPECT_EQ(handler.counters().auth_pubkey_success, 1u);
    EXPECT_EQ(handler.counters().auth_pubkey_fail, 0u);
}

TEST_F(TestSshServer, test_multiple_sessions)
{
    SshServer server("test-ssh-server");
    BasicSshServerHandler handler;

    handler.add_allowed_user("user1", "pass1");
    handler.add_allowed_user("user2", "pass2");

    ASSERT_TRUE(server.set_port(0));
    ASSERT_TRUE(server.set_rsa_key(HOST_KEY_PATH));
    server.set_server_handler(&handler);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ssh-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));
    int port = server.get_port();
    ASSERT_GT(port, 0);

    // First client
    SshSession client1;
    ASSERT_TRUE(client1.fast_connect("user1", "127.0.0.1", port));
    auto auth1 = client1.auth_password("pass1");
    ASSERT_TRUE(auth1.success());

    // Second client
    SshSession client2;
    ASSERT_TRUE(client2.fast_connect("user2", "127.0.0.1", port));
    auto auth2 = client2.auth_password("pass2");
    ASSERT_TRUE(auth2.success());

    SIHD_LOG(info, "Active sessions: {}", handler.session_count());
    EXPECT_EQ(handler.session_count(), 2u);

    client1.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(handler.session_count(), 1u);

    client2.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(handler.session_count(), 0u);

    server.stop();
    worker.stop_worker();

    EXPECT_EQ(handler.counters().auth_password_success, 2u);
    EXPECT_EQ(handler.counters().sessions_opened, 2u);
    EXPECT_EQ(handler.counters().sessions_closed, 2u);
}

TEST_F(TestSshServer, test_custom_auth_callback)
{
    SshServer server("test-ssh-server");
    BasicSshServerHandler handler;

    // Use custom callback instead of user list
    handler.set_auth_password_callback(
        [](SshSession *, std::string_view user, std::string_view password) -> bool {
            SIHD_LOG(info, "Custom auth callback: user={}, pass={}", user, password);
            return user == "dynamic" && password == "secret";
        });

    ASSERT_TRUE(server.set_port(0));
    ASSERT_TRUE(server.set_rsa_key(HOST_KEY_PATH));
    server.set_server_handler(&handler);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ssh-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));
    int port = server.get_port();
    ASSERT_GT(port, 0);

    // Client connects with dynamic credentials
    SshSession client;
    ASSERT_TRUE(client.fast_connect("dynamic", "127.0.0.1", port));
    auto auth = client.auth_password("secret");
    EXPECT_TRUE(auth.success());

    client.disconnect();

    server.stop();
    worker.stop_worker();

    EXPECT_EQ(handler.counters().auth_password_success, 1u);
}

} // namespace test
