#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/UdpSender.hpp>
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/Worker.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::net;
    class TestUdp:   public ::testing::Test
    {
        protected:
            TestUdp()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestUdp()
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



    TEST_F(TestUdp, test_udp_sender_receiver_connexion)
    {
        const char helloworld[] = "hello world";
        sihd::util::ArrChar array_rcv(40);

        UdpSender sender("127.0.0.1", 4242);
        UdpReceiver receiver(IpAddr::get_localhost(4242));

        EXPECT_TRUE(receiver.socket_opened());
        EXPECT_TRUE(receiver.socket().set_blocking(false));
        EXPECT_TRUE(sender.socket_opened());

        receiver.set_buffer(&array_rcv);

        EXPECT_EQ(sender.send(helloworld, sizeof(helloworld)), (ssize_t)sizeof(helloworld));
        EXPECT_EQ(receiver.receive(array_rcv), (ssize_t)sizeof(helloworld));
        EXPECT_TRUE(array_rcv.is_equal(helloworld, sizeof(helloworld) - 1));
        EXPECT_EQ(array_rcv.size(), sizeof(helloworld));
        
        const char hello[] = "hello";
        EXPECT_EQ(sender.send(hello, sizeof(hello)), (ssize_t)sizeof(hello));
        EXPECT_TRUE(receiver.poll(10));
        EXPECT_TRUE(array_rcv.is_equal(hello, sizeof(hello) - 1));
        EXPECT_EQ(array_rcv.size(), sizeof(hello));
    }

    TEST_F(TestUdp, test_udp_sender_receiver_broadcast)
    {
        const char helloworld[] = "hello world";
        sihd::util::ArrChar array_rcv(40);

        UdpSender sender;
        EXPECT_TRUE(sender.socket_opened());
        EXPECT_TRUE(sender.socket().set_broadcast(true));

        IpAddr any_ip(4242);
        UdpReceiver receiver(any_ip);
        EXPECT_TRUE(receiver.socket_opened());
        receiver.set_buffer(&array_rcv);

        IpAddr broadcast_ip("127.255.255.255", 4242, false);
        EXPECT_EQ(sender.send_to(broadcast_ip, helloworld, sizeof(helloworld)), (ssize_t)sizeof(helloworld));

        IpAddr rcv;
        receiver.receive_from(rcv);
        EXPECT_EQ(rcv.get_first_ipv4(), "127.0.0.1");

        EXPECT_TRUE(array_rcv.is_equal(helloworld, sizeof(helloworld) - 1));
        EXPECT_EQ(array_rcv.size(), sizeof(helloworld));
    }

}