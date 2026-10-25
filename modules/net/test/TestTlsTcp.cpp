#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>

#include "listener/make_listener.hpp"
#include "net_test_helpers.hpp"

#if !defined(__SIHD_WINDOWS__)
# include <sys/socket.h>
#else
# include <winsock2.h>
#endif

#include <wait_for.hpp>

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

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::net;
using namespace sihd::crypto;
constexpr int tls_connect_timeout_ms = 2000;

namespace
{

struct HandshakeOutcome
{
        bool client_ok = false;
        bool server_ok = false;
};

struct TlsPair
{
        Socket listener;
        Socket raw;
        Socket server_sock;
        TlsConnection server_conn;
        TlsConnection client_conn;
};

HandshakeOutcome handshake_pair(TlsPair & pair,
                                TlsContext & server_ctx,
                                TlsContext & client_ctx,
                                ZStringView server_name = "",
                                const std::shared_ptr<void> & resume = {})
{
    EXPECT_NO_FATAL_FAILURE(make_listener(pair.listener, IpAddr::localhost(0)));
    const int port = pair.listener.local_port().value_or(-1);
    EXPECT_GT(port, 0);

    EXPECT_TRUE(pair.raw.open(AF_INET, SOCK_STREAM, 0).has_value());
    EXPECT_TRUE(pair.raw.connect(IpAddr("127.0.0.1", port)).has_value());

    pair.server_sock = Socket(pair.listener.accept(tls_connect_timeout_ms).value_or(-1), true);
    EXPECT_TRUE(pair.server_conn.init(server_ctx, pair.server_sock).has_value());

    std::atomic<bool> server_ok {false};
    Worker worker([&] {
        server_ok = pair.server_conn.accept(tls_connect_timeout_ms).has_value();
        return true;
    });
    EXPECT_TRUE(worker.start_sync_worker("tls-accept"));

    EXPECT_TRUE(pair.client_conn.init(client_ctx, pair.raw).has_value());
    if (!server_name.empty())
    {
        EXPECT_TRUE(pair.client_conn.set_server_name(server_name).has_value());
    }
    if (resume)
    {
        EXPECT_TRUE(pair.client_conn.set_session(resume).has_value());
    }
    const bool client_ok = pair.client_conn.connect(tls_connect_timeout_ms).has_value();
    EXPECT_TRUE(worker.stop_worker());
    return {client_ok, server_ok};
}

HandshakeOutcome run_handshake(TlsContext & server_ctx, TlsContext & client_ctx, ZStringView server_name = "")
{
    TlsPair pair;
    return handshake_pair(pair, server_ctx, client_ctx, server_name);
}

void connect_tls_pair(TlsPair & pair, TlsContext & server_ctx, TlsContext & client_ctx)
{
    const HandshakeOutcome outcome = handshake_pair(pair, server_ctx, client_ctx);
    ASSERT_TRUE(outcome.server_ok);
    ASSERT_TRUE(outcome.client_ok);
}

void require_client_certificate(TlsContext & server_ctx,
                                TlsContext & client_ctx,
                                const Certificate & cert,
                                const PrivateKey & key,
                                const std::string & ca_path)
{
    ASSERT_TRUE(cert.save_pem(ca_path).has_value());
    ASSERT_TRUE(server_ctx.load_ca_cert(ca_path).has_value());
    ASSERT_TRUE(server_ctx.set_verify_peer(true).has_value());
    ASSERT_TRUE(client_ctx.set_certificate(cert).has_value());
    ASSERT_TRUE(client_ctx.set_private_key(key).has_value());
}

std::shared_ptr<void> handshake_and_session(TlsContext & server_ctx, TlsContext & client_ctx)
{
    TlsPair pair;
    const HandshakeOutcome full = handshake_pair(pair, server_ctx, client_ctx);
    EXPECT_TRUE(full.client_ok);
    EXPECT_TRUE(full.server_ok);

    // TLS 1.3 sends the tickets after the handshake: the client takes them on its next read
    EXPECT_EQ(pair.client_conn.session(), nullptr);
    EXPECT_TRUE(pair.raw.set_blocking(false).has_value());
    std::array<char, 8> buf {};
    EXPECT_FALSE(pair.client_conn.read(buf.data(), buf.size()).has_value());
    // freeing a connection without close_notify drops its session: shut it down first
    EXPECT_TRUE(pair.client_conn.shutdown().has_value());
    return pair.client_conn.session();
}

PrivateKey generate_key()
{
    PrivateKey key;
    (void)key.generate_rsa(2048);
    return key;
}

const PrivateKey & tls_key()
{
    static const PrivateKey key = generate_key();
    return key;
}

Certificate generate_cert()
{
    Certificate cert;
    (void)cert.generate_self_signed(tls_key(), "localhost");
    return cert;
}

const Certificate & tls_cert()
{
    static const Certificate cert = generate_cert();
    return cert;
}

} // namespace

class TestTlsTcp: public ::testing::Test
{
    protected:
        TestTlsTcp() { LoggerManager::stream(); }

        virtual ~TestTlsTcp() { LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            ASSERT_TRUE(_key);
            ASSERT_TRUE(_cert);

            (void)_server_ctx.init(true);
            ASSERT_TRUE(_server_ctx.set_certificate(_cert));
            ASSERT_TRUE(_server_ctx.set_private_key(_key));

            (void)_client_ctx.init(false);
            ASSERT_TRUE(_client_ctx.set_verify_peer(false));
        }

        const PrivateKey & _key = tls_key();
        const Certificate & _cert = tls_cert();
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
        for (auto & read_client : srv->read_activity())
        {
            if (!read_client->disconnected && !read_client->error)
                srv->send_to_client(read_client, read_client->read_array);
            std::lock_guard lk(read_client->mutex);
            if (read_client->read_array.is_bytes_equal(hello_arr) && !received_signaled.exchange(true))
                received_sync.sync(std::chrono::seconds(1));
        }
        if (srv->client_count() == 1u && !connected_signaled.exchange(true))
            connected_sync.sync(std::chrono::seconds(1));
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tls-server"));
    // bounds the join if an ASSERT aborts first
    Defer stop_worker([&] {
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
    ssize_t received_size = client.receive(recv_arr).value_or(0);
    ASSERT_GT(received_size, 0);
    EXPECT_EQ(std::string(recv_arr.data(), static_cast<size_t>(received_size)), "hello tls");

    const std::string payload(128 * 1024, 'x');
    ASSERT_TRUE(client.send_all({payload.data(), payload.size()}).has_value());
    std::string echoed;
    std::array<char, 8192> buffer;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (echoed.size() < payload.size() && std::chrono::steady_clock::now() < deadline)
    {
        if (!client.socket().tls_pending() && !client.poll(100))
            continue;
        const auto size = client.receive(buffer.data(), buffer.size());
        if (size && size.value() > 0)
            echoed.append(buffer.data(), size.value());
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
    Defer stop_worker([&] {
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
    TlsPair pair;
    ASSERT_NO_FATAL_FAILURE(connect_tls_pair(pair, _server_ctx, _client_ctx));

    // the timed handshake switches the mode itself and hands it back blocking
    EXPECT_EQ(pair.raw.is_blocking(), true);

    const char msg[] = "blocking handshake";
    ASSERT_GT((ssize_t)pair.client_conn.write(msg, sizeof(msg) - 1).value_or(0), 0);

    std::array<char, 32> recv_buf {};
    size_t total = 0;
    while (total < sizeof(msg) - 1)
    {
        ssize_t received_size = (ssize_t)pair.server_conn.read(recv_buf.data() + total, recv_buf.size() - total)
                                    .value_or(0);
        ASSERT_GT(received_size, 0);
        total += (size_t)received_size;
    }
    EXPECT_EQ(std::string(recv_buf.data(), total), "blocking handshake");
}

TEST_F(TestTlsTcp, tls_stale_openssl_error_does_not_fail_healthy_io)
{
    TlsPair pair;
    ASSERT_NO_FATAL_FAILURE(connect_tls_pair(pair, _server_ctx, _client_ctx));

    ASSERT_TRUE(pair.raw.set_blocking(false));

    std::array<char, 8> buf {};
    // consumes the session tickets the server sent after the handshake
    EXPECT_FALSE(pair.client_conn.read(buf.data(), buf.size()).has_value());

    ERR_raise(ERR_LIB_USER, 1);
    auto idle_read = pair.client_conn.read(buf.data(), buf.size());
    ASSERT_FALSE(idle_read.has_value());
    EXPECT_EQ(idle_read.error().code, ErrorCode::would_block);

    ERR_raise(ERR_LIB_USER, 1);
    EXPECT_EQ((ssize_t)pair.client_conn.write("x", 1).value_or(0), 1);
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestTlsTcp, tls_write_to_closed_peer_does_not_raise_sigpipe)
{
    ScopedSignalAction default_sigpipe(SIGPIPE, SIG_DFL);
    ASSERT_TRUE(default_sigpipe.installed());

    TlsPair pair;
    ASSERT_NO_FATAL_FAILURE(connect_tls_pair(pair, _server_ctx, _client_ctx));
    ASSERT_TRUE(set_reset_on_close(pair.server_sock));
    ASSERT_TRUE(pair.server_sock.close().has_value());

    std::array<char, 16> buf {};
    EXPECT_FALSE(pair.client_conn.read(buf.data(), buf.size()).has_value());
    EXPECT_FALSE(pair.client_conn.write("data", 4).has_value());
    EXPECT_TRUE(pair.client_conn.shutdown().has_value());
}
#endif

TEST_F(TestTlsTcp, tls_client_context_verifies_peer_by_default)
{
    TlsContext strict_client;
    ASSERT_TRUE(strict_client.init(false).has_value());

    const HandshakeOutcome outcome = run_handshake(_server_ctx, strict_client);
    EXPECT_FALSE(outcome.client_ok);
    EXPECT_FALSE(outcome.server_ok);
}

TEST_F(TestTlsTcp, tls_client_verifies_server_name)
{
    sihd::sys::TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string ca_path = sihd::sys::fs::combine(tmp_dir.path(), "ca.pem");
    ASSERT_TRUE(_cert.save_pem(ca_path).has_value());

    TlsContext strict_client;
    ASSERT_TRUE(strict_client.init(false).has_value());
    ASSERT_TRUE(strict_client.load_ca_cert(ca_path).has_value());

    EXPECT_TRUE(run_handshake(_server_ctx, strict_client, "localhost").client_ok);
    EXPECT_FALSE(run_handshake(_server_ctx, strict_client, "wrong.example").client_ok);
    EXPECT_FALSE(run_handshake(_server_ctx, strict_client, "127.0.0.1").client_ok);
}

TEST_F(TestTlsTcp, tls_server_requires_client_certificate_when_verifying)
{
    sihd::sys::TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string ca_path = sihd::sys::fs::combine(tmp_dir.path(), "client_ca.pem");
    ASSERT_TRUE(_cert.save_pem(ca_path).has_value());

    ASSERT_TRUE(_server_ctx.load_ca_cert(ca_path).has_value());
    ASSERT_TRUE(_server_ctx.set_verify_peer(true).has_value());

    const HandshakeOutcome outcome = run_handshake(_server_ctx, _client_ctx);
    EXPECT_FALSE(outcome.server_ok);
}

TEST_F(TestTlsTcp, tls_connection_without_init_fails)
{
    TlsConnection conn;
    EXPECT_FALSE(conn.connect(100).has_value());
    EXPECT_FALSE(conn.accept(100).has_value());
    EXPECT_FALSE(conn.accept_step().has_value());
    std::array<char, 16> buf {};
    EXPECT_FALSE(conn.read(buf.data(), buf.size()).has_value());
    EXPECT_FALSE(conn.write(buf.data(), buf.size()).has_value());
    EXPECT_FALSE(conn.pending());

    TlsContext empty_ctx;
    Socket sock;
    ASSERT_TRUE(sock.open(AF_INET, SOCK_STREAM, 0).has_value());
    auto inited = conn.init(empty_ctx, sock);
    ASSERT_FALSE(inited.has_value());
    EXPECT_EQ(inited.error().code, ErrorCode::not_initialized);
}

TEST_F(TestTlsTcp, tls_accept_step_drives_handshake_to_completion)
{
    Socket listener;
    make_listener(listener, IpAddr::localhost(0));
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);

    Socket client_sock;
    ASSERT_TRUE(client_sock.open(AF_INET, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(client_sock.connect(IpAddr("127.0.0.1", port)).has_value());

    TlsSocket server_side(listener.accept(tls_connect_timeout_ms).value_or(-1), true);
    ASSERT_TRUE(server_side.is_open());
    server_side.set_tls_context(_server_ctx);

    TlsConnection client_conn;
    ASSERT_TRUE(client_conn.init(_client_ctx, client_sock));
    Worker worker([&client_conn] { return client_conn.connect(tls_connect_timeout_ms).has_value(); });
    ASSERT_TRUE(worker.start_sync_worker("tls-client-handshake"));
    Defer stop_worker([&] { worker.stop_worker(); });

    auto step = server_side.tls_accept_step();
    ASSERT_TRUE(step.has_value());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    sihd::sys::Poll poll;
    poll.set_limit(1);
    while ((*step == TlsHandshakeStep::want_read || *step == TlsHandshakeStep::want_write)
           && std::chrono::steady_clock::now() < deadline)
    {
        if (*step == TlsHandshakeStep::want_read)
            poll.set_read_fd(server_side.socket());
        else
            poll.set_write_fd(server_side.socket());
        poll.poll(100);
        step = server_side.tls_accept_step();
        ASSERT_TRUE(step.has_value());
    }
    ASSERT_TRUE(worker.stop_worker());
    EXPECT_EQ(*step, TlsHandshakeStep::complete);
    EXPECT_TRUE(server_side.tls_active());

    const ArrChar hello("stepped");
    ASSERT_GT((ssize_t)client_conn.write(hello.data(), hello.size()).value_or(0), 0);
    ArrChar recv_arr(64);
    auto received = server_side.receive(recv_arr);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received.value(), (size_t)hello.size());

    EXPECT_TRUE(client_conn.shutdown().has_value());
    auto closed = server_side.receive(recv_arr);
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed.value(), 0u);
}

TEST_F(TestTlsTcp, tls_handshake_deadline_drops_silent_peer)
{
    TcpServer server("tls-server");
    BasicServerHandler server_handler;
    server_handler.set_tls_context(_server_ctx);
    ASSERT_TRUE(server_handler.set_tls_accept_timeout(300));

    ASSERT_TRUE(server.open_and_bind(IpAddr::localhost(0)).has_value());
    const int port = server.socket().local_port().value();
    ASSERT_GT(port, 0);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tls-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    Socket silent_peer;
    ASSERT_TRUE(silent_peer.open(AF_INET, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(silent_peer.connect(IpAddr("127.0.0.1", port)).has_value());

    wait_for([&] { return server_handler.clients().size() == 1u; });
    ASSERT_EQ(server_handler.clients().size(), 1u);

    wait_for([&] { return server_handler.clients().empty(); });
    EXPECT_EQ(server_handler.client_count(), 0u);
}

TEST_F(TestTlsTcp, tls_send_to_client_refused_while_partial_write_pending)
{
    TcpServer server("tls-server");
    TcpClient client("tls-client");
    BasicServerHandler server_handler;
    server_handler.set_tls_context(_server_ctx);
    client.set_tls_context(_client_ctx);

    ASSERT_TRUE(server.open_and_bind(IpAddr::localhost(0)).has_value());
    const int port = server.socket().local_port().value();
    ASSERT_GT(port, 0);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tls-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(IpAddr("127.0.0.1", port), tls_connect_timeout_ms).has_value());
    wait_for([&] { return server_handler.client_count() == 1u; });
    ASSERT_EQ(server_handler.client_count(), 1u);
    wait_for([&] { return client.socket().tls_active(); });

    const size_t payload_size = 16 * 1024 * 1024;
    ArrChar payload(payload_size);
    payload.resize(payload_size);
    for (size_t i = 0; i < payload_size; ++i)
        payload.buf()[i] = (char)(i % 251 + 1);
    auto clients = server_handler.clients();
    ASSERT_EQ(clients.size(), 1u);
    ASSERT_TRUE(wait_for([&] { return server_handler.send_to_client(clients[0], payload); }));

    const ArrChar second("refused");
    EXPECT_FALSE(server_handler.send_to_client(clients[0], second));

    ArrChar recv_arr(payload_size);
    size_t received = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (received < payload_size && std::chrono::steady_clock::now() < deadline)
    {
        if (!client.socket().tls_pending() && !client.poll(100))
            continue;
        const auto size = client.receive(recv_arr.buf() + received, payload_size - received);
        if (!size)
        {
            if (!size.error().retryable())
                break;
        }
        else if (size.value() > 0)
            received += size.value();
        else
            break;
    }
    ASSERT_EQ(received, payload_size);
    EXPECT_EQ(memcmp(recv_arr.buf(), payload.buf(), payload_size), 0);

    bool resumed = wait_for([&] { return server_handler.send_to_client(clients[0], second); });
    EXPECT_TRUE(resumed);
}

TEST_F(TestTlsTcp, tls_socket_connect_failure_closes_socket)
{
    Socket listener;
    make_listener(listener, IpAddr::localhost(0));
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);

    TlsSocket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    client.set_tls_context(_client_ctx);
    constexpr int timeout_ms = 300;
    auto connect_result = client.connect(IpAddr("127.0.0.1", port), timeout_ms);
    EXPECT_FALSE(connect_result.has_value());
    EXPECT_FALSE(client.is_open());

    (void)listener.close();
}

TEST_F(TestTlsTcp, tls_socket_standalone_accept)
{
    Socket listener;
    make_listener(listener, IpAddr::localhost(0));
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);

    TlsSocket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    client.set_tls_context(_client_ctx);

    std::unique_ptr<TlsSocket> server_side;
    Worker worker([&] {
        const int fd = listener.accept(tls_connect_timeout_ms).value_or(-1);
        if (fd < 0)
            return false;
        server_side = std::make_unique<TlsSocket>(fd, true);
        server_side->set_tls_context(_server_ctx);
        return server_side->tls_accept(tls_connect_timeout_ms).has_value();
    });
    ASSERT_TRUE(worker.start_sync_worker("tls-server-accept"));
    Defer stop_worker([&] { worker.stop_worker(); });

    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", port), tls_connect_timeout_ms).has_value());
    ASSERT_TRUE(worker.stop_worker());
    ASSERT_NE(server_side.get(), nullptr);

    EXPECT_TRUE(client.tls_active());
    EXPECT_TRUE(server_side->tls_active());

    const ArrChar hello("tls standalone");
    ASSERT_EQ(client.send(hello).value_or(0), (size_t)hello.size());
    ArrChar recv_arr(64);
    auto received = server_side->receive(recv_arr);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received.value(), (size_t)hello.size());
    EXPECT_EQ(strncmp(recv_arr.data(), hello.data(), hello.size()), 0);

    EXPECT_TRUE(server_side->shutdown().has_value());
    auto closed = client.receive(recv_arr);
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed.value(), 0u);

    (void)client.close();
    (void)server_side->close();
    (void)listener.close();
}

TEST_F(TestTlsTcp, tls_socket_plain_fallback_without_context)
{
    Socket listener;
    make_listener(listener, IpAddr::localhost(0));
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);

    TlsSocket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", port), tls_connect_timeout_ms).has_value());

    TlsSocket server_side(listener.accept(tls_connect_timeout_ms).value_or(-1), true);
    ASSERT_TRUE(server_side.is_open());
    EXPECT_FALSE(server_side.tls_active());

    const ArrChar hello("plain");
    ASSERT_EQ(client.send(hello).value_or(0), (size_t)hello.size());
    ArrChar recv_arr(64);
    EXPECT_EQ(server_side.receive(recv_arr), (ssize_t)hello.size());
    EXPECT_EQ(strncmp(recv_arr.data(), hello.data(), hello.size()), 0);

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();
}

TEST_F(TestTlsTcp, session_id_context_is_configurable)
{
    constexpr size_t session_id_context_max = 32;

    TlsContext ctx;
    EXPECT_FALSE(ctx.set_session_id_context("app").has_value());

    ASSERT_TRUE(ctx.init(true).has_value());
    EXPECT_TRUE(ctx.set_session_id_context("app").has_value());
    EXPECT_TRUE(ctx.set_session_id_context(std::string(session_id_context_max, 'x')).has_value());
    EXPECT_FALSE(ctx.set_session_id_context(std::string(session_id_context_max + 1, 'x')).has_value());

    const auto empty = ctx.set_session_id_context("");
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(empty.error().message, "session id context is empty");
}

TEST_F(TestTlsTcp, renegotiation_is_refused_unless_allowed)
{
    TlsContext ctx;
    EXPECT_FALSE(ctx.set_renegotiation(true).has_value());

    ASSERT_TRUE(ctx.init(true).has_value());
    SSL_CTX *native = static_cast<SSL_CTX *>(ctx.native());
    EXPECT_NE(SSL_CTX_get_options(native) & SSL_OP_NO_RENEGOTIATION, 0u);

    ASSERT_TRUE(ctx.set_renegotiation(true).has_value());
    EXPECT_EQ(SSL_CTX_get_options(native) & SSL_OP_NO_RENEGOTIATION, 0u);

    ASSERT_TRUE(ctx.set_renegotiation(false).has_value());
    EXPECT_NE(SSL_CTX_get_options(native) & SSL_OP_NO_RENEGOTIATION, 0u);
}

TEST_F(TestTlsTcp, tls_session_resumes_when_server_verifies_client)
{
    sihd::sys::TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    ASSERT_NO_FATAL_FAILURE(require_client_certificate(_server_ctx,
                                                       _client_ctx,
                                                       _cert,
                                                       _key,
                                                       sihd::sys::fs::combine(tmp_dir.path(), "client_ca.pem")));
    ASSERT_TRUE(_server_ctx.set_session_id_context("sihd-test").has_value());

    const std::shared_ptr<void> session = handshake_and_session(_server_ctx, _client_ctx);
    ASSERT_TRUE(session != nullptr);

    TlsPair resumed;
    const HandshakeOutcome outcome = handshake_pair(resumed, _server_ctx, _client_ctx, "", session);
    EXPECT_TRUE(outcome.client_ok);
    EXPECT_TRUE(outcome.server_ok);
    EXPECT_TRUE(resumed.client_conn.session_reused());
}

TEST_F(TestTlsTcp, tls_resume_fails_when_server_verifies_without_session_id_context)
{
    sihd::sys::TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    ASSERT_NO_FATAL_FAILURE(require_client_certificate(_server_ctx,
                                                       _client_ctx,
                                                       _cert,
                                                       _key,
                                                       sihd::sys::fs::combine(tmp_dir.path(), "client_ca.pem")));

    const std::shared_ptr<void> session = handshake_and_session(_server_ctx, _client_ctx);
    ASSERT_TRUE(session != nullptr);

    TlsPair resumed;
    const HandshakeOutcome outcome = handshake_pair(resumed, _server_ctx, _client_ctx, "", session);
    EXPECT_FALSE(outcome.client_ok && outcome.server_ok);
}

} // namespace test
