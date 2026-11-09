#include <gtest/gtest.h>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#include "ssh_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;
using namespace sihd::sys;

class TestSshServer: public ::testing::Test
{
    protected:
        TestSshServer() { LoggerManager::stream(); }

        virtual ~TestSshServer() { LoggerManager::clear_loggers(); }

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

    SshSession client;
    ASSERT_TRUE(client.fast_connect({.user = "testuser", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
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

TEST_F(TestSshServer, test_server_issue_banner)
{
    SshServer server("test-ssh-server");
    BasicSshServerHandler handler;

    handler.add_allowed_user("testuser", "testpass");

    ASSERT_TRUE(server.set_port(0));
    ASSERT_TRUE(server.set_rsa_key(HOST_KEY_PATH));
    ASSERT_TRUE(server.set_banner("Welcome to SIHD SSH\n"));
    server.set_server_handler(&handler);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ssh-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    int port = server.get_port();
    ASSERT_GT(port, 0);

    SshSession client;
    ASSERT_TRUE(client.fast_connect({.user = "testuser", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
    EXPECT_TRUE(client.connected());

    auto auth = client.auth_password("testpass");
    EXPECT_TRUE(auth.success());

    EXPECT_EQ(client.get_banner(), "Welcome to SIHD SSH\n");

    client.disconnect();

    server.stop();
    worker.stop_worker();
}

TEST_F(TestSshServer, test_pubkey_auth_success)
{
    SshKey client_pubkey;
    ASSERT_TRUE(client_pubkey.import_pubkey_file(CLIENT_PUBKEY_PATH).has_value());
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

    SshSession client;
    ASSERT_TRUE(client.fast_connect({.user = "testuser", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
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

    SshSession client1;
    ASSERT_TRUE(client1.fast_connect({.user = "user1", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
    auto auth1 = client1.auth_password("pass1");
    ASSERT_TRUE(auth1.success());

    SshSession client2;
    ASSERT_TRUE(client2.fast_connect({.user = "user2", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
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

    handler.set_auth_password_callback([](SshSession *, std::string_view user, std::string_view password) -> bool {
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

    SshSession client;
    ASSERT_TRUE(client.fast_connect({.user = "dynamic", .host = "127.0.0.1", .port = port, .process_config = false})
                    .has_value());
    auto auth = client.auth_password("secret");
    EXPECT_TRUE(auth.success());

    client.disconnect();

    server.stop();
    worker.stop_worker();

    EXPECT_EQ(handler.counters().auth_password_success, 1u);
}

TEST_F(TestSshServer, test_password_auth_third_attempt)
{
    auto test_server = make_test_server("test-ssh-server-attempts");
    ASSERT_NE(test_server, nullptr);

    SshSession client;
    ASSERT_TRUE(
        client
            .fast_connect({.user = "testuser", .host = "127.0.0.1", .port = test_server->port, .process_config = false})
            .has_value());

    EXPECT_TRUE(client.auth_password("wrong1").denied());
    EXPECT_TRUE(client.auth_password("wrong2").denied());
    EXPECT_TRUE(client.auth_password("testpass").success());

    client.disconnect();
    test_server->stop();

    EXPECT_EQ(test_server->handler.counters().auth_password_fail, 2u);
    EXPECT_EQ(test_server->handler.counters().auth_password_success, 1u);
}

TEST_F(TestSshServer, test_exec_forwards_stdin)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "no cat on windows";

    auto test_server = make_test_server("test-ssh-server-exec-stdin");
    ASSERT_NE(test_server, nullptr);
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_exec("cat").has_value());

    // larger than a pipe: the server feeds the child while it forwards its output
    const std::string input(300 * 1024, 'x');
    ASSERT_EQ(channel.write(ArrCharView(input.data(), input.size())), (int)input.size());
    ASSERT_TRUE(channel.send_eof().has_value());
    // a slow client: the output outgrows its window before it reads
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::string output;
    ArrChar chunk;
    chunk.reserve(64 * 1024);
    while (channel.read_timeout(chunk, 5000) > 0)
        output.append(chunk.data(), chunk.size());

    EXPECT_EQ(output.size(), input.size());
    EXPECT_TRUE(output == input);
}

} // namespace test
