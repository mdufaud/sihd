#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/Socket.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::net;
    class TestSocket:   public ::testing::Test
    {
        protected:
            TestSocket()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestSocket()
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

    TEST_F(TestSocket, test_socket)
    {
        IpAddr local_ip = {"127.0.0.1", 4200};
        Socket sock_rcv;
        Socket sock_send;
        const char buff[] = "hello world";
        char rcv_buff[12];

        EXPECT_FALSE(sock_rcv.is_open());
        EXPECT_EQ(sock_rcv.socket(), -1);
        EXPECT_TRUE(sock_rcv.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        EXPECT_FALSE(sock_rcv.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        EXPECT_FALSE(sock_rcv.open(AF_INET, SOCK_STREAM, IPPROTO_TCP));

        EXPECT_TRUE(sock_send.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

        EXPECT_TRUE(sock_rcv.bind(local_ip));
        EXPECT_EQ(sock_send.send_to(local_ip, (void *)buff, sizeof(buff)), 12);
        EXPECT_EQ(sock_rcv.receive_from(local_ip, rcv_buff, 12), 12);
        EXPECT_EQ(strcmp(buff, rcv_buff), 0);
    }
}