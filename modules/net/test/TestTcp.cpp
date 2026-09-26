#include <chrono>
#include <cstring>
#include <thread>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ObserverWaiter.hpp>
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
        TestTcp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTcp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTcp, test_tcp_server)
{
    IpAddr localhost = IpAddr::localhost(4242);

    sihd::util::ArrChar hello_world_arr("hello world");
    sihd::util::ArrChar welcome_arr("welcome");

    TcpServer server("tcp-server");
    TcpClient client1("tcp-client-1");
    TcpClient client2("tcp-client-2");
    TcpClient client3("tcp-client-3");
    TcpClient client4("tcp-client-4");
    TcpClient client5("tcp-client-5");

    BasicServerHandler server_handler;
    ObserverWaiter observer_count(&server_handler);

    server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    server.set_queue_size(3);
    server_handler.set_max_clients(4);
    sihd::util::Handler<BasicServerHandler *> handler([&welcome_arr](BasicServerHandler *srv) {
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
    sihd::util::Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    SIHD_LOG(debug, "Simulating a new connection");
    client1.open_and_connect(localhost, connect_timeout_ms);

    wait_for([&] { return server_handler.client_count() == 1u; });

    SIHD_LOG(debug, "Simulating a send from client: {}", hello_world_arr.str());
    EXPECT_TRUE(client1.send_all(hello_world_arr));

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

    client2.open_and_connect(localhost, connect_timeout_ms);
    client3.open_and_connect(localhost, connect_timeout_ms);
    client4.open_and_connect(localhost, connect_timeout_ms);

    wait_for([&] { return server_handler.client_count() == 4u; });

    SIHD_LOG(debug, "Simulating a new unacceptable connection");
    client5.open_and_connect(localhost, connect_timeout_ms);

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
    IpAddr localhost = IpAddr::localhost(4242);
    sihd::util::ArrChar hello("hello world");
    sihd::util::ArrChar bye("bye");
    sihd::util::ArrChar recv(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    // client connect
    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms));
    EXPECT_TRUE(client.socket_opened());
    EXPECT_TRUE(client.connected());

    // server accept
    int accepted_socket = server.accept(connect_timeout_ms);
    Socket accepted(accepted_socket);

    // client send hello world
    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv));

    // client receive bye
    sihd::util::Handler<INetReceiver *> handler([&recv](INetReceiver *rcv) {
        rcv->receive(recv);
        SIHD_LOG(debug, "Data received: {} - {} bytes", recv.str(), recv.byte_size());
    });
    client.add_observer(&handler);
    EXPECT_EQ(accepted.send(bye), (ssize_t)bye.size());
    EXPECT_TRUE(client.poll(1));
    EXPECT_TRUE(bye.is_equal(recv));

    // shutdown and close
    accepted.shutdown();
    EXPECT_EQ(client.receive(recv), 0);
    EXPECT_FALSE(client.connected());
    EXPECT_TRUE(client.close());
    EXPECT_TRUE(accepted.close());
    EXPECT_TRUE(server.close());
}
TEST_F(TestTcp, test_tcp_client_reads_data_before_peer_hangup)
{
    IpAddr localhost = IpAddr::localhost(4246);
    TcpClient client("tcp-client");
    Socket server;
    make_listener(server, localhost);

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms));
    Socket accepted(server.accept(connect_timeout_ms));
    ASSERT_TRUE(accepted.is_open());

    const std::string payload(128 * 1024, 'x');
    std::string received;
    Handler<INetReceiver *> handler([&](INetReceiver *receiver) {
        sihd::util::ArrByte buffer(4096);
        ssize_t size;
        while ((size = receiver->receive(buffer)) > 0)
            received.append(reinterpret_cast<const char *>(buffer.buf()), static_cast<size_t>(size));
    });
    client.add_observer(&handler);

    ASSERT_TRUE(accepted.send_all({payload.data(), payload.size()}));
    ASSERT_TRUE(accepted.shutdown());
    ASSERT_TRUE(client.poll(1000));

    EXPECT_EQ(received, payload);
    EXPECT_FALSE(client.connected());
    accepted.close();
    server.close();
}

TEST_F(TestTcp, test_tcp_connect_timeout)
{
    IpAddr localhost = IpAddr::localhost(4243);
    sihd::util::ArrChar hello("hello");
    sihd::util::ArrChar recv(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms));
    EXPECT_TRUE(client.connected());

    int accepted_socket = server.accept(connect_timeout_ms);
    Socket accepted(accepted_socket);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv));

    EXPECT_TRUE(client.close());
    EXPECT_TRUE(accepted.close());
    EXPECT_TRUE(server.close());
}

TEST_F(TestTcp, test_tcp_reconnect)
{
    IpAddr localhost = IpAddr::localhost(4244);
    sihd::util::ArrChar hello("hello");
    sihd::util::ArrChar recv(20);
    TcpClient client("tcp-client");
    Socket server;

    make_listener(server, localhost);

    EXPECT_TRUE(client.open_and_connect(localhost, connect_timeout_ms));
    EXPECT_TRUE(client.connected());

    int accepted_socket = server.accept(connect_timeout_ms);
    Socket accepted(accepted_socket);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted.receive(recv), (ssize_t)hello.size());

    accepted.close();
    EXPECT_TRUE(client.close());
    EXPECT_FALSE(client.connected());

    EXPECT_TRUE(client.reconnect(connect_timeout_ms));
    EXPECT_TRUE(client.connected());

    int accepted_socket2 = server.accept(connect_timeout_ms);
    Socket accepted2(accepted_socket2);

    EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
    EXPECT_EQ(accepted2.receive(recv), (ssize_t)hello.size());
    EXPECT_TRUE(hello.is_equal(recv));

    EXPECT_TRUE(client.close());
    EXPECT_TRUE(accepted2.close());
    EXPECT_TRUE(server.close());
}

TEST_F(TestTcp, test_tcp_terminal_connect_error_is_not_retryable)
{
    Socket listener;
    ASSERT_TRUE(listener.open(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    ASSERT_TRUE(listener.bind(IpAddr("127.0.0.1", 0)));
    const int port = listener.local_port();
    ASSERT_GT(port, 0);
    ASSERT_TRUE(listener.close());

    TcpClient client("tcp-client");
    EXPECT_FALSE(client.open_and_connect(IpAddr("127.0.0.1", port), connect_timeout_ms));
    EXPECT_FALSE(client.connected());
    EXPECT_FALSE(client.socket().retryable());
    client.close();
}

TEST_F(TestTcp, test_tcp_connect_timeout_unreachable)
{
    // Network policy determines whether this times out or fails immediately.
    IpAddr unreachable("192.0.2.1", 81);
    TcpClient client("tcp-client");

    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.open_and_connect(unreachable, connect_timeout_ms));
    EXPECT_FALSE(client.connected());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, std::chrono::milliseconds(5 * connect_timeout_ms));

    client.close();
}

TEST_F(TestTcp, test_tcp_server_poll_limit)
{
    IpAddr localhost = IpAddr::localhost(4245);
    sihd::util::ArrChar hello("hello");
    sihd::util::ArrChar recv(20);
    TcpServer server("tcp-server");
    TcpClient client1("tcp-client-1");
    TcpClient client2("tcp-client-2");

    BasicServerHandler server_handler;

    server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    // the listener takes one slot, only one client can be polled
    server.set_poll_limit(2);
    EXPECT_FALSE(server.set_queue_size(70000));

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tcp-server"));
    // bounds the join if an ASSERT aborts first
    sihd::util::Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    EXPECT_TRUE(client1.open_and_connect(localhost, connect_timeout_ms));
    EXPECT_TRUE(client2.open_and_connect(localhost, connect_timeout_ms));

    wait_for([&] { return server_handler.client_count() == 1u; });
    EXPECT_EQ(server_handler.client_count(), 1u);

    // the second client was never polled: it gets closed instead of hanging
    client2.socket().set_blocking(false);
    wait_for([&] { return client2.receive(recv) == 0; });
    EXPECT_EQ(client2.receive(recv), 0);

    EXPECT_TRUE(client1.send_all(hello));
    wait_for([&] {
        auto all_clients = server_handler.clients();
        if (all_clients.size() != 1)
            return false;
        std::lock_guard lk(all_clients[0]->mutex);
        return all_clients[0]->read_array.is_bytes_equal(hello);
    });

    client1.close();
    wait_for([&] { return server_handler.client_count() == 0u; });
    EXPECT_EQ(server_handler.client_count(), 0u);

    EXPECT_TRUE(server.stop());
    EXPECT_TRUE(worker.stop_worker());

    EXPECT_FALSE(client1.poll(1));
}

TEST_F(TestTcp, test_tcp_server_partial_write)
{
    IpAddr localhost = IpAddr::localhost(4246);
    TcpServer server("tcp-server");
    TcpClient client("tcp-client");
    BasicServerHandler server_handler;

    server.open_and_bind(localhost);
    server.set_server_handler(&server_handler);
    server.set_poll_timeout(1);
    Worker worker([&server] { return server.start(); });
    ASSERT_TRUE(worker.start_sync_worker("tcp-server"));
    // bounds the join if an ASSERT aborts first
    sihd::util::Defer stop_worker([&] {
        server.stop();
        worker.stop_worker();
    });
    ASSERT_TRUE(server.wait_ready(std::chrono::seconds(1)));

    ASSERT_TRUE(client.open_and_connect(localhost, connect_timeout_ms));
    wait_for([&] { return server_handler.client_count() == 1u; });
    ASSERT_EQ(server_handler.client_count(), 1u);

    // more than a socket send buffer: delivery must go through the partial write offset
    const size_t payload_size = 2 * 1024 * 1024;
    sihd::util::ArrChar payload(payload_size);
    payload.resize(payload_size);
    for (size_t i = 0; i < payload_size; ++i)
        payload.buf()[i] = (char)(i % 251 + 1);
    auto clients = server_handler.clients();
    ASSERT_EQ(clients.size(), 1u);
    ASSERT_TRUE(server_handler.send_to_client(clients[0], payload));

    // reads are served while the write is pending: they cannot starve each other
    const sihd::util::ArrChar ping("ping");
    EXPECT_TRUE(client.send_all(ping));
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

    sihd::util::ArrChar recv(payload_size);
    size_t received = 0;
    // non-blocking with a deadline: a dead server must fail the test, not hang it
    ASSERT_TRUE(client.socket().set_blocking(false));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (received < payload_size)
    {
        ssize_t r = client.receive(recv.buf() + received, payload_size - received);
        if (r < 0)
        {
            ASSERT_TRUE(client.socket().retryable());
            ASSERT_LT(std::chrono::steady_clock::now(), deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        ASSERT_GT(r, 0);
        received += (size_t)r;
    }
    EXPECT_EQ(received, payload_size);
    EXPECT_EQ(memcmp(recv.buf(), payload.buf(), payload_size), 0);

    ASSERT_TRUE(server.stop());
    ASSERT_TRUE(worker.stop_worker());
}

} // namespace test
