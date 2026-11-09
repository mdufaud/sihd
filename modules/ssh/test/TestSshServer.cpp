#include <array>
#include <atomic>
#include <chrono>

#include <gtest/gtest.h>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/SshSubsystemFunction.hpp>
#include <sihd/ssh/SshSubsystemPty.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
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

namespace
{

// stays open a while after the client end of file, counting its notifications
class EofCountingFunction: public SshSubsystemFunction
{
    public:
        EofCountingFunction(std::string_view command, std::atomic<int> & eofs):
            SshSubsystemFunction(command),
            _eofs(eofs)
        {
        }

        void on_eof() override
        {
            if (++_eofs == 1)
                _close_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        }

        bool is_running() const override { return _eofs == 0 || std::chrono::steady_clock::now() < _close_at; }

    private:
        std::atomic<int> & _eofs;
        std::chrono::steady_clock::time_point _close_at;
};

} // namespace

class TestSshServer: public ::testing::Test
{
    protected:
        TestSshServer() { LoggerManager::stream(); }

        virtual ~TestSshServer() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static std::string read_until_closed(SshChannel & channel)
        {
            std::string output;
            ArrChar chunk;
            chunk.reserve(64 * 1024);
            while (channel.read_timeout(chunk, 5000) > 0)
                output.append(chunk.data(), chunk.size());
            return output;
        }
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

TEST_F(TestSshServer, test_pubkey_auth_refuses_a_key_missing_from_authorized_keys)
{
    SshKey client_pubkey;
    ASSERT_TRUE(client_pubkey.import_pubkey_file(CLIENT_PUBKEY_PATH).has_value());
    SshKey host_pubkey;
    ASSERT_TRUE(host_pubkey.import_pubkey_file(HOST_PUBKEY_PATH).has_value());
    sihd::sys::TmpDir tmp_dir;
    const std::string authorized_keys = sihd::sys::fs::combine(tmp_dir.path(), "authorized_keys");
    ASSERT_TRUE(sihd::sys::fs::write(authorized_keys, "ssh-rsa " + host_pubkey.base64() + " host\n").has_value());

    SshServerHelper test_server("test-ssh-server-authorized-keys-refused");
    test_server.handler.add_allowed_pubkey("testuser", client_pubkey.base64());
    ASSERT_TRUE(test_server.server.set_authorized_keys_file(authorized_keys).has_value());
    ASSERT_TRUE(test_server.start());
    SshSession client;
    ASSERT_TRUE(
        client
            .fast_connect({.user = "testuser", .host = "127.0.0.1", .port = test_server.port, .process_config = false})
            .has_value());

    const auto auth = client.auth_key_file(CLIENT_KEY_PATH);

    EXPECT_FALSE(auth.success());
    EXPECT_EQ(test_server.handler.counters().auth_pubkey_success, 0u);
}

TEST_F(TestSshServer, test_pubkey_auth_accepts_a_key_listed_in_authorized_keys)
{
    SshKey client_pubkey;
    ASSERT_TRUE(client_pubkey.import_pubkey_file(CLIENT_PUBKEY_PATH).has_value());
    sihd::sys::TmpDir tmp_dir;
    const std::string authorized_keys = sihd::sys::fs::combine(tmp_dir.path(), "authorized_keys");
    const std::string listing = "# keys\n\nno-pty,no-agent-forwarding ssh-ed25519 " + client_pubkey.base64()
                                + " client\n";
    ASSERT_TRUE(sihd::sys::fs::write(authorized_keys, listing).has_value());

    SshServerHelper test_server("test-ssh-server-authorized-keys-listed");
    test_server.handler.add_allowed_pubkey("testuser", client_pubkey.base64());
    ASSERT_TRUE(test_server.server.set_authorized_keys_file(authorized_keys).has_value());
    ASSERT_TRUE(test_server.start());
    SshSession client;
    ASSERT_TRUE(
        client
            .fast_connect({.user = "testuser", .host = "127.0.0.1", .port = test_server.port, .process_config = false})
            .has_value());

    const auto auth = client.auth_key_file(CLIENT_KEY_PATH);

    EXPECT_TRUE(auth.success());
    EXPECT_EQ(test_server.handler.counters().auth_pubkey_success, 1u);
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

TEST_F(TestSshServer, test_clients_stalled_in_key_exchange_leave_the_server_serving)
{
    auto server = make_test_server();
    ASSERT_NE(server, nullptr);
    std::array<SilentTcpClient, 3> silent_clients;
    for (auto & silent : silent_clients)
        ASSERT_TRUE(silent.connect(server->port));

    Stopwatch stopwatch;
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*server, client));

    EXPECT_LT(stopwatch.time(), Duration(time::sec(2)));
}

TEST_F(TestSshServer, test_client_stalled_past_the_kex_timeout_is_dropped)
{
    SshServerHelper server;
    ASSERT_TRUE(server.server.set_kex_timeout(1));
    ASSERT_TRUE(server.start());

    SilentTcpClient silent;
    ASSERT_TRUE(silent.connect(server.port));

    EXPECT_TRUE(silent.closed_by_server(Duration(time::sec(3))));
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

    const std::string output = read_until_closed(channel);

    EXPECT_EQ(output.size(), input.size());
    EXPECT_TRUE(output == input);
}

TEST_F(TestSshServer, test_handler_gets_the_client_eof_once)
{
    auto test_server = make_test_server("test-ssh-server-eof-once");
    ASSERT_NE(test_server, nullptr);
    std::atomic<int> eofs = 0;
    test_server->handler.set_exec_handler_callback(
        [&eofs]([[maybe_unused]] SshSession *session,
                [[maybe_unused]] SshChannel *channel,
                std::string_view command,
                [[maybe_unused]] bool has_pty,
                [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *function = new EofCountingFunction(command, eofs);
            function->set_callback([](const std::string &) {
                return SshSubsystemFunction::Result {.output = {}, .error_output = {}, .exit_code = 0};
            });
            return function;
        });
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_exec("eof").has_value());

    ASSERT_TRUE(channel.send_eof().has_value());
    (void)read_until_closed(channel);

    EXPECT_EQ(eofs, 1);
}

TEST_F(TestSshServer, test_function_output_larger_than_the_window)
{
    auto test_server = make_test_server("test-ssh-server-function-window");
    ASSERT_NE(test_server, nullptr);
    // far more than the client window: the server session never blocks on a full window
    const std::string expected(8 * 1024 * 1024, 'x');
    test_server->handler.set_exec_handler_callback(
        [&expected]([[maybe_unused]] SshSession *session,
                    [[maybe_unused]] SshChannel *channel,
                    std::string_view command,
                    [[maybe_unused]] bool has_pty,
                    [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *function = new SshSubsystemFunction(command);
            function->set_callback([&expected](const std::string &) {
                return SshSubsystemFunction::Result {.output = expected, .error_output = {}, .exit_code = 0};
            });
            return function;
        });
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());

    ASSERT_TRUE(channel.request_exec("big").has_value());
    const std::string output = read_until_closed(channel);

    EXPECT_EQ(output.size(), expected.size());
    EXPECT_TRUE(output == expected);
}

TEST_F(TestSshServer, test_buffered_exec_output_larger_than_the_window)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the command reads /dev/zero";

    auto test_server = make_test_server("test-ssh-server-buffered-exec-window");
    ASSERT_NE(test_server, nullptr);
    test_server->handler.set_exec_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           std::string_view command,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *exec = new SshSubsystemExec(command);
            exec->set_fork_mode(false);
            return exec;
        });
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());

    // sent at once when the child ends, to a client that does not read yet: far more than its window
    ASSERT_TRUE(channel.request_exec("head -c 262144 /dev/zero").has_value());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const std::string output = read_until_closed(channel);

    EXPECT_EQ(output.size(), 256u * 1024);
    EXPECT_TRUE(output == std::string(256 * 1024, 0));
}

TEST_F(TestSshServer, test_exec_output_streams_without_waiting_the_poll_timeout)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the command reads /dev/zero";

    auto test_server = make_test_server("test-ssh-server-exec-stream");
    ASSERT_NE(test_server, nullptr);
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(*test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());

    Stopwatch stopwatch;
    ASSERT_TRUE(channel.request_exec("head -c 8388608 /dev/zero").has_value());
    const std::string output = read_until_closed(channel);
    const Duration elapsed = stopwatch.time();

    EXPECT_EQ(output.size(), 8u * 1024 * 1024);
    if constexpr (build::is_run_with_sanitizer == false)
    {
        EXPECT_LT(elapsed, Duration(time::sec(2)));
    }
}

TEST_F(TestSshServer, test_pty_output_larger_than_the_window)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the pty shell is /bin/sh";

    SshServerHelper test_server("test-ssh-server-pty-window");
    test_server.handler.set_shell_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *pty = new SshSubsystemPty();
            pty->set_shell("/bin/sh");
            // the shell outlives its output
            pty->set_args({"-c", "seq 1 15000; sleep 1"});
            return pty;
        });
    ASSERT_TRUE(test_server.start());
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_pty().has_value());
    ASSERT_TRUE(channel.request_shell().has_value());

    std::string expected;
    for (int i = 1; i <= 15000; ++i)
        expected += std::to_string(i) + "\r\n";

    // the server reads the pty 4 KiB per 100 ms poll: in 3 s the output outgrows a window nobody reads
    std::this_thread::sleep_for(std::chrono::seconds(3));
    const std::string output = read_until_closed(channel);

    EXPECT_EQ(output.size(), expected.size());
    EXPECT_TRUE(output == expected);
}

TEST_F(TestSshServer, test_pty_output_left_at_shell_exit_reaches_the_client)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the pty shell is /bin/sh";

    SshServerHelper test_server("test-ssh-server-pty-exit");
    test_server.handler.set_shell_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *pty = new SshSubsystemPty();
            pty->set_shell("/bin/sh");
            pty->set_args({"-c", "seq 1 15000"});
            return pty;
        });
    ASSERT_TRUE(test_server.start());
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_pty().has_value());
    ASSERT_TRUE(channel.request_shell().has_value());

    std::string expected;
    for (int i = 1; i <= 15000; ++i)
        expected += std::to_string(i) + "\r\n";

    // the shell exits with its last lines still in the pty
    const std::string output = read_until_closed(channel);

    EXPECT_EQ(output.size(), expected.size());
    EXPECT_TRUE(output == expected);
}

TEST_F(TestSshServer, test_pty_eof_follows_the_input_the_pty_has_not_taken_yet)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the pty shell is /bin/sh";

    SshServerHelper test_server("test-ssh-server-pty-pending-eof");
    test_server.handler.set_shell_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *pty = new SshSubsystemPty();
            pty->set_shell("/bin/sh");
            pty->set_args({"-c", "stty -echo; wc -c"});
            return pty;
        });
    ASSERT_TRUE(test_server.start());

    SshSession client;
    ASSERT_TRUE(connect_to_test_server(test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_pty().has_value());
    ASSERT_TRUE(channel.request_shell().has_value());

    std::string input;
    for (int line = 0; line < 2000; ++line)
        input += std::string(99, 'x') + "\n";

    // far more than the pty buffers: the server keeps the rest pending when the eof arrives
    ASSERT_EQ(channel.write(ArrCharView(input.data(), input.size())), (int)input.size());
    ASSERT_TRUE(channel.send_eof().has_value());
    const std::string output = read_until_closed(channel);

    // what was echoed before stty ran varies: the count is the last line
    EXPECT_TRUE(output.ends_with("200000\r\n")) << output.substr(output.size() > 64 ? output.size() - 64 : 0);
}

TEST_F(TestSshServer, test_pty_shell_runs_input_sent_before_eof)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the pty shell is /bin/sh";

    SshServerHelper test_server("test-ssh-server-pty-eof");
    test_server.handler.set_shell_handler_callback(
        []([[maybe_unused]] SshSession *session,
           [[maybe_unused]] SshChannel *channel,
           [[maybe_unused]] bool has_pty,
           [[maybe_unused]] const WinSize & winsize) -> ISshSubsystemHandler * {
            auto *pty = new SshSubsystemPty();
            pty->set_shell("/bin/sh");
            return pty;
        });
    ASSERT_TRUE(test_server.start());
    SshSession client;
    ASSERT_TRUE(connect_to_test_server(test_server, client));
    SshChannel channel;
    ASSERT_TRUE(client.make_channel_session(channel).has_value());
    ASSERT_TRUE(channel.request_pty().has_value());
    ASSERT_TRUE(channel.request_shell().has_value());

    // the eof follows the script at once: the shell must still run its last commands
    const std::string script = "sleep 1\nexit 5\n";
    ASSERT_EQ(channel.write(ArrCharView(script.data(), script.size())), (int)script.size());
    ASSERT_TRUE(channel.send_eof().has_value());
    ArrChar chunk;
    chunk.reserve(4096);
    while (channel.read_timeout(chunk, 5000) > 0)
        ;

    EXPECT_EQ(channel.exit_status(), 5);
}

} // namespace test
