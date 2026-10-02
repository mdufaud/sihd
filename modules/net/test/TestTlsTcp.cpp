#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/platform.hpp>

#include "listener/make_listener.hpp"

#if !defined(__SIHD_WINDOWS__)
# include <sys/socket.h>
#else
# include <winsock2.h>
#endif

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Worker.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::net;
using namespace sihd::crypto;
constexpr int tls_connect_timeout_ms = 2000;

class TestTlsTcp: public ::testing::Test
{
    protected:
        TestTlsTcp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTlsTcp() { sihd::util::LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            ASSERT_TRUE(_key.generate_rsa(2048));
            ASSERT_TRUE(_cert.generate_self_signed(_key, "localhost"));

            (void)_server_ctx.init(true);
            ASSERT_TRUE(_server_ctx.set_certificate(_cert));
            ASSERT_TRUE(_server_ctx.set_private_key(_key));

            (void)_client_ctx.init(false);
            _client_ctx.set_verify_peer(false);
        }

        PrivateKey _key;
        Certificate _cert;
        TlsContext _server_ctx;
        TlsContext _client_ctx;
};

TEST_F(TestTlsTcp, test_tls_tcp_send_receive)
{
    IpAddr localhost = IpAddr::localhost(4343);

    ArrChar hello_arr("hello tls");

    TcpServer server("tls-server");
    TcpClient client("tls-client");

    BasicServerHandler server_handler;
    EXPECT_FALSE(server_handler.set_tls_accept_timeout(-1));
    server_handler.set_tls_context(_server_ctx);
    client.set_tls_context(_client_ctx);

    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    // the handler runs on the server thread on every activity; it signals each
    // synchronizer once when the real condition is met, the test waits on them
    Synchronizer connected_sync(2);
    Synchronizer received_sync(2);
    std::atomic<bool> connected_signaled {false};
    std::atomic<bool> received_signaled {false};

    Handler<BasicServerHandler *> handler([&](BasicServerHandler *srv) {
        for (auto & c : srv->read_activity())
        {
            if (!c->disconnected && !c->error)
                srv->send_to_client(c, c->read_array);
            std::lock_guard lk(c->mutex);
            if (c->read_array.is_bytes_equal(hello_arr) && !received_signaled.exchange(true))
                received_sync.sync(std::chrono::seconds(1));
        }
        if (srv->client_count() == 1u && !connected_signaled.exchange(true))
            connected_sync.sync(std::chrono::seconds(1));
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tls-server"));
    // bounds the join if an ASSERT aborts first
    sihd::util::Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, tls_connect_timeout_ms).has_value());

    ASSERT_TRUE(connected_sync.sync(std::chrono::seconds(2)));
    EXPECT_EQ(server_handler.client_count(), 1u);

    EXPECT_TRUE(client.send_all(hello_arr).has_value());

    ASSERT_TRUE(received_sync.sync(std::chrono::seconds(2)));

    EXPECT_TRUE(client.poll(500));
    ArrChar recv_arr(64);
    ssize_t rcv = client.receive(recv_arr).value_or(0);
    ASSERT_GT(rcv, 0);
    EXPECT_EQ(std::string(recv_arr.data(), static_cast<size_t>(rcv)), "hello tls");

    const std::string payload(128 * 1024, 'x');
    ASSERT_TRUE(client.send_all({payload.data(), payload.size()}).has_value());
    std::string echoed;
    char buffer[8192];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (echoed.size() < payload.size() && std::chrono::steady_clock::now() < deadline)
    {
        if (!client.socket().tls_pending() && !client.poll(100))
            continue;
        const auto size = client.receive(buffer, sizeof(buffer));
        if (size && size.value() > 0)
            echoed.append(buffer, size.value());
        else if (!size && !size.error().retryable())
            break;
    }
    EXPECT_EQ(echoed, payload);

    (void)client.close();
    EXPECT_TRUE(server.stop());
    EXPECT_TRUE(worker.stop_worker());
}

TEST_F(TestTlsTcp, tls_silent_peer_does_not_block_another_client)
{
    IpAddr localhost = IpAddr::localhost(4346);
    TcpServer server("tls-server");
    TcpClient client("tls-client");
    BasicServerHandler server_handler;
    server_handler.set_tls_context(_server_ctx);
    Synchronizer connected_sync(2);
    std::atomic<bool> connected_signaled {false};
    Handler<BasicServerHandler *> connected_handler([&](BasicServerHandler *srv) {
        if (srv->client_count() == 1u && !connected_signaled.exchange(true))
            connected_sync.sync(std::chrono::seconds(1));
    });
    server_handler.add_observer(&connected_handler);
    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tls-server"));
    sihd::util::Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    Socket silent_peer;
    ASSERT_TRUE(silent_peer.open(AF_INET, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(silent_peer.connect(localhost).has_value());

    client.set_tls_context(_client_ctx);
    ASSERT_TRUE(client.open_and_connect(localhost, tls_connect_timeout_ms).has_value());
    ASSERT_TRUE(connected_sync.sync(std::chrono::seconds(2)));
    EXPECT_EQ(server_handler.client_count(), 1u);

    (void)client.close();
    (void)silent_peer.close();
}

TEST_F(TestTlsTcp, tls_connect_timeout_on_non_tls_peer)
{
    IpAddr localhost = IpAddr::localhost(4344);

    // plain TCP listener that completes the TCP handshake but never speaks TLS:
    // the client's SSL handshake must time out instead of hanging forever
    Socket listener;
    make_listener(listener, localhost);

    TcpClient client("tls-client");
    client.set_tls_context(_client_ctx);

    constexpr int timeout_ms = 500;
    auto start = std::chrono::steady_clock::now();
    bool ok = client.open_and_connect(localhost, timeout_ms).has_value();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                       .count();

    EXPECT_FALSE(ok);
    EXPECT_LT(elapsed, 5 * timeout_ms);

    (void)client.close();
    (void)listener.close();
}

TEST_F(TestTlsTcp, tls_timed_handshake_on_blocking_socket)
{
    IpAddr localhost = IpAddr::localhost(4345);

    Socket listener;
    make_listener(listener, localhost);

    // the timed handshake switches the mode itself and hands it back blocking
    Socket raw;
    ASSERT_TRUE(raw.open(AF_INET, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(raw.connect(localhost).has_value());

    Socket server_sock(listener.accept(tls_connect_timeout_ms).value_or(-1), true);
    ASSERT_TRUE(server_sock.is_open());

    TlsConnection server_conn;
    ASSERT_TRUE(server_conn.init(_server_ctx, server_sock).has_value());

    Worker worker([&server_conn] { return server_conn.accept(tls_connect_timeout_ms).has_value(); });
    ASSERT_TRUE(worker.start_sync_worker("tls-accept"));
    // bounds the join if an ASSERT aborts first
    sihd::util::Defer stop_worker([&] { worker.stop_worker(); });

    TlsConnection client_conn;
    ASSERT_TRUE(client_conn.init(_client_ctx, raw));
    ASSERT_TRUE(client_conn.connect(tls_connect_timeout_ms).has_value());

    ASSERT_TRUE(worker.stop_worker());

    EXPECT_TRUE(raw.is_blocking());

    const char msg[] = "blocking handshake";
    ASSERT_GT((ssize_t)client_conn.write(msg, sizeof(msg) - 1).value_or(0), 0);

    char recv_buf[32] = {0};
    size_t total = 0;
    while (total < sizeof(msg) - 1)
    {
        ssize_t rcv = (ssize_t)server_conn.read(recv_buf + total, sizeof(recv_buf) - total).value_or(0);
        ASSERT_GT(rcv, 0);
        total += (size_t)rcv;
    }
    EXPECT_EQ(std::string(recv_buf, total), "blocking handshake");
}

} // namespace test
