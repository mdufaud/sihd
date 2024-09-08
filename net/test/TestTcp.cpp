#include <gtest/gtest.h>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ObserverWaiter.hpp>
#include <sihd/util/Worker.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::net;
class TestTcp: public ::testing::Test
{
    protected:
        TestTcp() { sihd::util::LoggerManager::basic(); }

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
        for (BasicServerHandler::Client *client : srv->new_clients())
        {
            SIHD_LOG(info, "Server new client: {}", client->fd());
            srv->send_to_client(client, welcome_arr);
        }
        for (BasicServerHandler::Client *client : srv->read_activity())
        {
            SIHD_LOG(info, "Client said: {}", client->read_array.str(','));
        }
        for (BasicServerHandler::Client *client : srv->write_activity())
        {
            SIHD_LOG(info, "Server wrote to client: {}", client->write_array.str(','));
        }
    });
    server_handler.add_observer(&handler);

    Worker worker([&server] { return server.start(); });
    EXPECT_TRUE(worker.start_sync_worker("tcp-server"));

    SIHD_LOG(debug, "Simulating a new connection");
    client1.open_and_connect(localhost);

    usleep(1000);

    SIHD_LOG(debug, "Simulating a send from client: {}", hello_world_arr.str());
    EXPECT_TRUE(client1.send_all(hello_world_arr));

    usleep(1000);

    EXPECT_EQ(server_handler.clients().size(), 1u);
    if (server_handler.clients().size() == 1)
    {
        BasicServerHandler::Client *client = server_handler.clients()[0];
        EXPECT_TRUE(client->read_array.is_bytes_equal(hello_world_arr));
    }

    SIHD_LOG(debug, "Simulating 3 new connections");

    client2.open_and_connect(localhost);
    client3.open_and_connect(localhost);
    client4.open_and_connect(localhost);

    usleep(2000);

    SIHD_LOG(debug, "Simulating a new unacceptable connection");
    client5.open_and_connect(localhost);

    usleep(1000);

    // connection not accepted
    EXPECT_EQ(server_handler.clients().size(), 4u);
    if (server_handler.clients().size() == 4u)
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

    EXPECT_TRUE(server.open(AF_INET, SOCK_STREAM, 0));
    EXPECT_TRUE(server.set_reuseaddr(true));
    EXPECT_TRUE(server.bind(localhost));
    EXPECT_TRUE(server.listen(2));

    // client connect
    EXPECT_TRUE(client.open_and_connect(localhost));
    EXPECT_TRUE(client.socket_opened());
    EXPECT_TRUE(client.connected());

    // server accept
    int accepted_socket = server.accept();
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
} // namespace test
