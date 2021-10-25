#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/TcpClient.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::net;
    class TestTcpClient:   public ::testing::Test
    {
        protected:
            TestTcpClient()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestTcpClient()
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

    TEST_F(TestTcpClient, test_tcp_client)
    {
        sihd::util::ArrChar hello("hello world", sizeof("hello world"));
        sihd::util::ArrChar bye("bye", sizeof("bye"));
        sihd::util::ArrChar recv(20);
        Socket server;
        IpAddr localhost = IpAddr::get_localhost(4242);

        EXPECT_TRUE(server.open(AF_INET, SOCK_STREAM, 0));
        EXPECT_TRUE(server.set_reuseaddr(true));
        EXPECT_TRUE(server.bind(localhost));
        EXPECT_TRUE(server.listen(2));

        // client connect
        TcpClient client(localhost);
        EXPECT_TRUE(client.socket_opened());

        // server accept
        int accepted_socket = server.accept();
        Socket accepted(accepted_socket);

        // client send hello world
        EXPECT_EQ(client.send(hello), (ssize_t)hello.size());
        EXPECT_EQ(accepted.receive(recv), (ssize_t)hello.size());
        EXPECT_TRUE(hello.is_equal(recv));

        // client receive bye
        client.set_buffer(&recv);
        EXPECT_EQ(accepted.send(bye), (ssize_t)bye.size());
        EXPECT_EQ(client.receive(), (ssize_t)bye.size());
        EXPECT_TRUE(bye.is_equal(recv));

        // shutdown and close
        accepted.shutdown();
        EXPECT_EQ(client.receive(recv), 0);
        EXPECT_TRUE(client.close());
        EXPECT_TRUE(accepted.close());
        EXPECT_TRUE(server.close());
    }
}