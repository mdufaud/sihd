#include <gtest/gtest.h>

#include <chrono>

#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <sys/socket.h>
#else
# include <winsock2.h>
#endif

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
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

            _server_ctx.init(true);
            ASSERT_TRUE(_server_ctx.set_certificate(_cert));
            ASSERT_TRUE(_server_ctx.set_private_key(_key));

            _client_ctx.init(false);
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
    server_handler.set_tls_context(_server_ctx);
    client.set_tls_context(_client_ctx);

    server.open_and_bind(localhost);
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
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, tls_connect_timeout_ms));

    ASSERT_TRUE(connected_sync.sync(std::chrono::seconds(2)));
    EXPECT_EQ(server_handler.client_count(), 1u);

    EXPECT_TRUE(client.send_all(hello_arr));

    ASSERT_TRUE(received_sync.sync(std::chrono::seconds(2)));

    EXPECT_TRUE(client.poll(500));
    ArrChar recv_arr(64);
    ssize_t rcv = client.receive(recv_arr);
    ASSERT_GT(rcv, 0);
    EXPECT_EQ(std::string(recv_arr.data(), static_cast<size_t>(rcv)), "hello tls");

    client.close();
    EXPECT_TRUE(server.stop());
    EXPECT_TRUE(worker.stop_worker());
}

TEST_F(TestTlsTcp, tls_connect_timeout_on_non_tls_peer)
{
    IpAddr localhost = IpAddr::localhost(4344);

    // plain TCP listener that completes the TCP handshake but never speaks TLS:
    // the client's SSL handshake must time out instead of hanging forever
    Socket listener;
    ASSERT_TRUE(listener.open(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(listener.set_reuseaddr(true));
    ASSERT_TRUE(listener.bind(localhost));
    ASSERT_TRUE(listener.listen(2));

    TcpClient client("tls-client");
    client.set_tls_context(_client_ctx);

    constexpr int timeout_ms = 500;
    auto start = std::chrono::steady_clock::now();
    bool ok = client.open_and_connect(localhost, timeout_ms);
    auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    EXPECT_FALSE(ok);
    EXPECT_LT(elapsed, 5 * timeout_ms);

    client.close();
    listener.close();
}

} // namespace test
