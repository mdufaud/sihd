#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/util/ObserverWaiter.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    using namespace sihd::net;
    class TestTcp:  public ::testing::Test
    {
        protected:
            TestTcp()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestTcp()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestTcp, test_tcp_server)
    {
        sihd::util::ArrStr hello_world_arr("hello world");
        sihd::util::ArrStr welcome_arr("welcome");
        IpAddr localhost = IpAddr::get_localhost(4242);
        TcpServer server(localhost);
        BasicServerHandler server_handler;
        ObserverWaiter handler_waiter(&server_handler);

        server.set_server_handler(&server_handler);
        server.set_poll_timeout(1);
        server.set_queue_size(3);
        server_handler.set_max_clients(4);
        sihd::util::Handler<BasicServerHandler *> handler([this, &welcome_arr] (BasicServerHandler *bsh)
        {
            for (BasicServerHandler::Client *client: bsh->new_clients())
            {
                LOG(info, "Server new client: " << client->fd());
                bsh->send_to_client(client, welcome_arr);
            }
            for (BasicServerHandler::Client *client: bsh->read_activity())
            {
                LOG(info, "Client said: " << client->read_array->to_string(','));
            }
            for (BasicServerHandler::Client *client: bsh->write_activity())
            {
                LOG(info, "Server wrote to client: " << client->write_array->to_string(','));
            }
        });
        server_handler.add_observer(&handler);

        Worker worker(&server);
        EXPECT_TRUE(worker.start_worker("tcp-server"));

        usleep(1000);

        LOG(debug, "Simulating a new connection");
        TcpClient client(localhost);

        usleep(1000);

        LOG(debug, "Simulating a send from client: " << hello_world_arr.to_string());
        EXPECT_TRUE(client.send_all(hello_world_arr));

        usleep(1000);

        EXPECT_EQ(server_handler.clients().size(), 1u);
        if (server_handler.clients().size() == 1)
        {
            BasicServerHandler::Client *client = server_handler.clients()[0];
            EXPECT_TRUE(client->read_array->is_equal(hello_world_arr));
        }

        LOG(debug, "Simulating 3 new connections");
        TcpClient client2(localhost);
        TcpClient client3(localhost);
        TcpClient client4(localhost);

        usleep(2000);

        LOG(debug, "Simulating a new unacceptable connection");
        TcpClient client5(localhost);

        usleep(1000);

        // connection not accepted
        EXPECT_EQ(server_handler.clients().size(), 4u);
        if (server_handler.clients().size() == 4u)
        {
            // disconnected
            EXPECT_EQ(client5.receive(hello_world_arr), 0);
        }

        LOG(debug, "Stop serving");
        EXPECT_TRUE(server.stop_serving());
        EXPECT_TRUE(worker.stop_worker());

        LOG(debug, "Total observations: " << handler_waiter.notifications);
        // should be lower than ~11 in general
        EXPECT_LT(handler_waiter.notifications, 15);
    }

    TEST_F(TestTcp, test_tcp_client)
    {
        sihd::util::ArrChar hello("hello world", sizeof("hello world"));
        sihd::util::ArrChar bye("bye", sizeof("bye"));
        sihd::util::ArrStr recv(20);
        Socket server;
        IpAddr localhost = IpAddr::get_localhost(4242);

        EXPECT_TRUE(server.open(AF_INET, SOCK_STREAM, 0));
        EXPECT_TRUE(server.set_reuseaddr(true));
        EXPECT_TRUE(server.bind(localhost));
        EXPECT_TRUE(server.listen(2));

        // client connect
        TcpClient client(localhost);
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
        sihd::util::Handler<INetReceiver *> handler([&recv] (INetReceiver *rcv)
        {
            rcv->receive(recv);
            LOG(debug, "Data received: " << recv.to_string() << " - " << recv.byte_size() << " bytes");
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
}