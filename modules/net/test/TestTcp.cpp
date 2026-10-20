#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ObserverWaiter.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Worker.hpp>

#include "listener/make_listener.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::net;
constexpr int connect_timeout_ms = 1000;

class TestTcp: public ::testing::Test
{
    protected:
        TestTcp() { LoggerManager::stream(); }

        virtual ~TestTcp() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTcp, test_tcp_server)
{
    IpAddr localhost = IpAddr::localhost(4242);

    ArrChar hello_world_arr("hello world");
    ArrChar welcome_arr("welcome");

    TcpServer server("tcp-server");
    TcpClient client1("tcp-client-1");
    TcpClient client2("tcp-client-2");
    TcpClient client3("tcp-client-3");
    TcpClient client4("tcp-client-4");
    TcpClient client5("tcp-client-5");

    BasicServerHandler server_handler;
    ObserverWaiter observer_count(&server_handler);

    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    server.set_queue_size(3);
    server_handler.set_max_clients(4);
    Handler<BasicServerHandler *> handler([&welcome_arr](BasicServerHandler *srv) {
        for (auto & client : srv->new_clients())
        {
            SIHD_LOG(info, "Server new client: {}", client->fd());
            srv->send_to_client(client, welcome_arr);
        }
        for (auto & client : srv->read_activity())
        {
            SIHD_LOG(info, "Client said: {}", client->read_array.str(','));
        }
        for (auto & client : srv->write_activity())
        {
            SIHD_LOG(info, "Server wrote to client: {}", client->write_array.str(','));
        }
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tcp-server"));
    // bounds the join if an ASSERT aborts first
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    SIHD_LOG(debug, "Simulating a new connection");
    (void)client1.open_and_connect(localhost, connect_timeout_ms);

    wait_for([&] { return server_handler.client_count() == 1u; });

    SIHD_LOG(debug, "Simulating a send from client: {}", hello_world_arr.str());
    EXPECT_TRUE(client1.send_all(hello_world_arr).has_value());

    wait_for([&] {
        auto all_clients = server_handler.clients();
        if (all_clients.size() != 1)
            return false;
        std::lock_guard lk(all_clients[0]->mutex);
        return all_clients[0]->read_array.is_bytes_equal(hello_world_arr);
    });

    EXPECT_EQ(server_handler.client_count(), 1u);
    {
        auto all_clients = server_handler.clients();
        if (all_clients.size() == 1)
        {
            std::lock_guard lk(all_clients[0]->mutex);
            EXPECT_TRUE(all_clients[0]->read_array.is_bytes_equal(hello_world_arr));
        }
    }

    SIHD_LOG(debug, "Simulating 3 new connections");

    (void)client2.open_and_connect(localhost, connect_timeout_ms);
    (void)client3.open_and_connect(localhost, connect_timeout_ms);
    (void)client4.open_and_connect(localhost, connect_timeout_ms);

    wait_for([&] { return server_handler.client_count() == 4u; });

    SIHD_LOG(debug, "Simulating a new unacceptable connection");
    (void)client5.open_and_connect(localhost, connect_timeout_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // connection not accepted
    EXPECT_EQ(server_handler.client_count(), 4u);
    if (server_handler.client_count() == 4u)
    {
        // disconnected
        EXPECT_EQ(client5.receive(hello_world_arr), 0);
    }

    SIHD_LOG(debug, "Stop serving");
    EXPECT_TRUE(server.stop());
    EXPECT_TRUE(worker.stop_worker());

    SIHD_LOG(debug, "Total observations: {}", observer_count.notifications());
    // should be lower than ~11 in general
    EXPECT_LT(observer_count.notifications(), 15u);
}

TEST_F(TestTcp, test_tcp_client)
{
    IpAddr localhost = IpAddr::localhost(4247);
    ArrChar hello("hello world");
    ArrChar bye("bye");
    ArrChar recv_arr(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    // client connect
    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    EXPECT_TRUE(client.socket_opened());
    EXPECT_TRUE(client.connected());

    // server accept
    int accepted_socket = server.accept(connect_timeout_ms).value_or(-1);
    Socket accepted(accepted_socket);

    // client send hello world
    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv_arr), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv_arr));

    // client receive bye
    Handler<INetReceiver *> handler([&recv_arr](INetReceiver *receiver) {
        (void)receiver->receive(recv_arr);
        SIHD_LOG(debug, "Data received: {} - {} bytes", recv_arr.str(), recv_arr.byte_size());
    });
    client.add_observer(&handler);
    EXPECT_EQ(accepted.send(bye), (ssize_t)bye.size());
    EXPECT_TRUE(client.poll(1));
    EXPECT_TRUE(bye.is_equal(recv_arr));

    // shutdown and close
    (void)accepted.shutdown();
    EXPECT_EQ(client.receive(recv_arr), 0);
    EXPECT_FALSE(client.connected());
    EXPECT_TRUE(client.close().has_value());
    EXPECT_TRUE(accepted.close().has_value());
    EXPECT_TRUE(server.close().has_value());
}
TEST_F(TestTcp, test_tcp_client_reads_data_before_peer_hangup)
{
    IpAddr localhost = IpAddr::localhost(4246);
    TcpClient client("tcp-client");
    Socket server;
    make_listener(server, localhost);

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    Socket accepted(server.accept(connect_timeout_ms).value_or(-1));
    ASSERT_TRUE(accepted.is_open());

    const std::string payload(128 * 1024, 'x');
    std::string received;
    Handler<INetReceiver *> handler([&](INetReceiver *receiver) {
        ArrByte buffer(4096);
        ssize_t size;
        while ((size = receiver->receive(buffer).value_or(0)) > 0)
            received.append(reinterpret_cast<const char *>(buffer.buf()), static_cast<size_t>(size));
    });
    client.add_observer(&handler);

    ASSERT_TRUE(accepted.send_all({payload.data(), payload.size()}).has_value());
    ASSERT_TRUE(accepted.shutdown().has_value());
    ASSERT_TRUE(client.poll(1000));

    EXPECT_EQ(received, payload);
    EXPECT_FALSE(client.connected());
    (void)accepted.close();
    (void)server.close();
}

TEST_F(TestTcp, test_tcp_connect_timeout)
{
    IpAddr localhost = IpAddr::localhost(4243);
    ArrChar hello("hello");
    ArrChar recv_arr(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    EXPECT_TRUE(client.connected());

    int accepted_socket = server.accept(connect_timeout_ms).value_or(-1);
    Socket accepted(accepted_socket);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv_arr), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv_arr));

    EXPECT_TRUE(client.close().has_value());
    EXPECT_TRUE(accepted.close().has_value());
    EXPECT_TRUE(server.close().has_value());
}

TEST_F(TestTcp, test_tcp_reconnect)
{
    IpAddr localhost = IpAddr::localhost(4244);
    ArrChar hello("hello");
    ArrChar recv_arr(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    EXPECT_TRUE(client.connected());

    int accepted_socket = server.accept(connect_timeout_ms).value_or(-1);
    Socket accepted(accepted_socket);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv_arr), (ssize_t)hello.size());

    (void)accepted.close();
    EXPECT_TRUE(client.close().has_value());
    EXPECT_FALSE(client.connected());

    EXPECT_TRUE(client.reconnect(connect_timeout_ms).has_value());
    EXPECT_TRUE(client.connected());

    int accepted_socket2 = server.accept(connect_timeout_ms).value_or(-1);
    Socket accepted2(accepted_socket2);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted2.receive(recv_arr), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv_arr));

    EXPECT_TRUE(client.close().has_value());
    EXPECT_TRUE(accepted2.close().has_value());
    EXPECT_TRUE(server.close().has_value());
}

TEST_F(TestTcp, test_tcp_terminal_connect_error_is_not_retryable)
{
    Socket listener;
    ASSERT_TRUE(listener.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(listener.bind(IpAddr("127.0.0.1", 0)).has_value());
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);
    ASSERT_TRUE(listener.close().has_value());

    TcpClient client("tcp-client");
    auto connected = client.open_and_connect(IpAddr("127.0.0.1", port), connect_timeout_ms);
    EXPECT_FALSE(connected.has_value());
    EXPECT_FALSE(client.connected());
    EXPECT_FALSE(connected.error().retryable());
    (void)client.close();
}

TEST_F(TestTcp, test_tcp_connect_timeout_unreachable)
{
    // Network policy determines whether this times out or fails immediately.
    IpAddr unreachable("192.0.2.1", 81);
    TcpClient client("tcp-client");

    constexpr int timeout_ms = 200;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.open_and_connect(unreachable, timeout_ms).has_value());
    EXPECT_FALSE(client.connected());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, std::chrono::milliseconds(5 * timeout_ms));

    (void)client.close();
}

TEST_F(TestTcp, test_tcp_server_poll_limit)
{
    IpAddr localhost = IpAddr::localhost(4245);
    ArrChar hello("hello");
    ArrChar recv_arr(20);
    TcpServer server("tcp-server");
    TcpClient client1("tcp-client-1");
    TcpClient client2("tcp-client-2");

    BasicServerHandler server_handler;

    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    // the listener takes one slot, only one client can be polled
    server.set_poll_limit(2);
    EXPECT_FALSE(server.set_queue_size(70000));

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tcp-server"));
    // bounds the join if an ASSERT aborts first
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    EXPECT_TRUE(client1.open_and_connect(localhost, connect_timeout_ms).has_value());
    EXPECT_TRUE(client2.open_and_connect(localhost, connect_timeout_ms).has_value());

    wait_for([&] { return server_handler.client_count() == 1u; });
    EXPECT_EQ(server_handler.client_count(), 1u);

    // the second client was never polled: it gets closed instead of hanging
    (void)client2.socket().set_blocking(false);
    wait_for([&] { return client2.receive(recv_arr) == 0; });
    EXPECT_EQ(client2.receive(recv_arr), 0);

    EXPECT_TRUE(client1.send_all(hello).has_value());
    wait_for([&] {
        auto all_clients = server_handler.clients();
        if (all_clients.size() != 1)
            return false;
        std::lock_guard lk(all_clients[0]->mutex);
        return all_clients[0]->read_array.is_bytes_equal(hello);
    });

    (void)client1.close();
    wait_for([&] { return server_handler.client_count() == 0u; });
    EXPECT_EQ(server_handler.client_count(), 0u);

    EXPECT_TRUE(server.stop());
    EXPECT_TRUE(worker.stop_worker());

    EXPECT_FALSE(client1.poll(1));
}

TEST_F(TestTcp, test_tcp_server_default_poll_limit_follows_max_clients)
{
    constexpr size_t default_max_clients = 12;
    IpAddr localhost = IpAddr::localhost(4251);
    TcpServer server("tcp-server");
    BasicServerHandler server_handler;

    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    Synchronizer connected_sync(2);
    std::atomic<bool> connected_signaled {false};
    Handler<BasicServerHandler *> handler([&](BasicServerHandler *srv) {
        if (srv->client_count() == default_max_clients && !connected_signaled.exchange(true))
            connected_sync.sync(std::chrono::seconds(1));
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tcp-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    std::vector<std::unique_ptr<TcpClient>> clients;
    for (size_t i = 0; i < default_max_clients; ++i)
    {
        clients.push_back(std::make_unique<TcpClient>("tcp-client"));
        ASSERT_TRUE(clients.back()->open_and_connect(localhost, connect_timeout_ms).has_value());
    }

    ASSERT_TRUE(connected_sync.sync(std::chrono::seconds(2)));
    EXPECT_EQ(server_handler.client_count(), default_max_clients);
}

TEST_F(TestTcp, test_tcp_server_clients_use_keepalive)
{
    IpAddr localhost = IpAddr::localhost(4261);
    TcpServer server("tcp-server");
    TcpClient client("tcp-client");
    BasicServerHandler server_handler;

    ASSERT_TRUE(server.open_and_bind(localhost).has_value());
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    Synchronizer connected_sync(2);
    std::atomic<bool> keepalive {false};
    Handler<BasicServerHandler *> handler([&](BasicServerHandler *srv) {
        for (auto & new_client : srv->new_clients())
        {
            keepalive = new_client->socket.is_keepalive().value_or(false);
            connected_sync.sync(std::chrono::seconds(1));
        }
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tcp-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    ASSERT_TRUE(connected_sync.sync(std::chrono::seconds(2)));
    EXPECT_TRUE(keepalive);
}

TEST_F(TestTcp, test_tcp_server_drops_idle_clients)
{
    IpAddr localhost = IpAddr::localhost(4262);
    TcpServer server("tcp-server");
    TcpClient client("tcp-client");
    BasicServerHandler server_handler;

    ASSERT_TRUE(server.open_and_bind(localhost).has_value());
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    ASSERT_TRUE(server_handler.set_idle_timeout(500));
    EXPECT_FALSE(server_handler.set_idle_timeout(-1));

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tcp-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    const ArrChar ping("ping");
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(client.send_all(ping).has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(server_handler.client_count(), 1u);

    client.set_poll_timeout(3000);
    ASSERT_TRUE(client.poll());
    ArrChar recv_arr(20);
    EXPECT_EQ(client.receive(recv_arr), 0);
    EXPECT_EQ(server_handler.client_count(), 0u);
}

TEST_F(TestTcp, test_tcp_server_partial_write)
{
    IpAddr localhost = IpAddr::localhost(4248);
    TcpServer server("tcp-server");
    TcpClient client("tcp-client");
    BasicServerHandler server_handler;

    (void)server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tcp-server"));
    // bounds the join if an ASSERT aborts first
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    wait_for([&] { return server_handler.client_count() == 1u; });
    ASSERT_EQ(server_handler.client_count(), 1u);

    // more than a socket send buffer: delivery must go through the partial write offset
    const size_t payload_size = 2 * 1024 * 1024;
    ArrChar payload(payload_size);
    payload.resize(payload_size);
    for (size_t i = 0; i < payload_size; ++i)
        payload.buf()[i] = (char)(i % 251 + 1);
    auto clients = server_handler.clients();
    ASSERT_EQ(clients.size(), 1u);
    ASSERT_TRUE(server_handler.send_to_client(clients[0], payload));

    // reads are served while the write is pending: they cannot starve each other
    const ArrChar ping("ping");
    EXPECT_TRUE(client.send_all(ping).has_value());
    wait_for([&] {
        auto all_clients = server_handler.clients();
        if (all_clients.size() != 1)
            return false;
        std::lock_guard lk(all_clients[0]->mutex);
        return all_clients[0]->read_array.is_bytes_equal(ping);
    });
    {
        auto all_clients = server_handler.clients();
        ASSERT_EQ(all_clients.size(), 1u);
        std::lock_guard lk(all_clients[0]->mutex);
        ASSERT_TRUE(all_clients[0]->read_array.is_bytes_equal(ping));
    }

    ArrChar recv_arr(payload_size);
    size_t received = 0;
    // non-blocking with a deadline: a dead server must fail the test, not hang it
    ASSERT_TRUE(client.socket().set_blocking(false));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (received < payload_size)
    {
        auto receive_result = client.receive(recv_arr.buf() + received, payload_size - received);
        if (!receive_result)
        {
            ASSERT_LT(std::chrono::steady_clock::now(), deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const ssize_t received_bytes = (ssize_t)*receive_result;
        ASSERT_GT(received_bytes, 0);
        received += (size_t)received_bytes;
    }
    EXPECT_EQ(received, payload_size);
    EXPECT_EQ(memcmp(recv_arr.buf(), payload.buf(), payload_size), 0);

    ASSERT_TRUE(server.stop());
    ASSERT_TRUE(worker.stop_worker());
}

TEST_F(TestTcp, test_tcp_client_recv_timeout_never_parks)
{
    Socket listener;
    make_listener(listener, IpAddr::localhost(0));
    const int port = listener.local_port().value();
    ASSERT_GT(port, 0);
    const IpAddr localhost = IpAddr::localhost(port);

    TcpClient client("tcp-client");
    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms).has_value());
    Socket accepted(listener.accept(connect_timeout_ms).value_or(-1));
    ASSERT_TRUE(accepted.is_open());

    ASSERT_TRUE(client.set_recv_timeout(50));
    ArrChar recv_arr(64);
    const auto start = std::chrono::steady_clock::now();
    auto timeout_result = client.receive(recv_arr);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_FALSE(timeout_result.has_value());
    EXPECT_LT(elapsed, std::chrono::seconds(2));
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 25);

    const ArrChar first("one byte");
    ASSERT_TRUE(accepted.send_all(first).has_value());
    EXPECT_TRUE(client.poll(1000));
    auto first_read = client.receive(recv_arr);
    ASSERT_TRUE(first_read.has_value());
    EXPECT_EQ(first_read.value(), (size_t)first.size());

    const ArrChar second("second");
    ASSERT_TRUE(accepted.send_all(second).has_value());
    EXPECT_TRUE(client.poll(1000));
    IpAddr peer;
    auto with_addr = client.receive(peer, recv_arr);
    ASSERT_TRUE(with_addr.has_value());
    EXPECT_EQ(with_addr.value(), (size_t)second.size());
    EXPECT_EQ(peer, localhost);

    EXPECT_TRUE(client.close().has_value());

    (void)accepted.close();
    (void)listener.close();
}

TEST_F(TestTcp, test_tcp_unix_roundtrip)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "AF_UNIX stream is not supported by wine";

    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine({dir.path(), "tcp.sock"});

    TcpServer server("tcp-unix-server");
    TcpClient client("tcp-unix-client");
    BasicServerHandler server_handler;

    ASSERT_TRUE(server.open_unix_and_bind(path).has_value());
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);

    const ArrChar hello("hello unix");
    Synchronizer received_sync(2);
    std::atomic<bool> received_signaled {false};
    Handler<BasicServerHandler *> handler([&](BasicServerHandler *srv) {
        for (auto & read_client : srv->read_activity())
        {
            std::lock_guard lk(read_client->mutex);
            if (read_client->read_array.is_bytes_equal(hello) && !received_signaled.exchange(true))
                received_sync.sync(std::chrono::seconds(1));
        }
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tcp-unix-server"));
    Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_unix_and_connect(path).has_value());
    EXPECT_TRUE(client.connected());
    ASSERT_TRUE(client.send_all(hello).has_value());
    EXPECT_TRUE(received_sync.sync(std::chrono::seconds(2)));
}

} // namespace test
