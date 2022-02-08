#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/UdpSender.hpp>
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/net/INetReceiver.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/ObserverWaiter.hpp>

namespace test
{
    SIHD_LOGGER;
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

    TEST_F(TestUdp, test_udp_receiver_run)
    {
        sihd::util::ArrInt array_send = {1, 2, 3, 4, 5, 6};
        sihd::util::ArrInt array_rcv(array_send.size());
        IpAddr localhost("127.0.0.1", 4242);
        UdpSender sender("udp-sender");
        UdpReceiver receiver("udp-receiver");

        EXPECT_TRUE(sender.open_and_connect(localhost));
        EXPECT_TRUE(receiver.open_and_bind(localhost));

        sihd::util::ObserverWaiter obs(&receiver);

        ssize_t receive_ret = -1;
        sihd::util::Handler<INetReceiver *> handler([&receive_ret, &array_rcv] (INetReceiver *rcv)
        {
            receive_ret = rcv->receive(array_rcv);
            SIHD_LOG(debug, "Data received: " << array_rcv.to_string(' ') << " - " << array_rcv.byte_size() << " bytes");
        });
        receiver.add_observer(&handler);
        receiver.set_poll_timeout(1);
        sihd::util::Worker worker(&receiver);
        SIHD_LOG(debug, "Starting receiver");
        EXPECT_TRUE(worker.start_sync_worker("receiver"));

        SIHD_LOG(debug, "Sending: " << array_send.to_string(',') << " (" << array_send.byte_size() << " bytes)");
        EXPECT_EQ(sender.send(array_send), (ssize_t)array_send.byte_size());

        usleep(1000);

        EXPECT_TRUE(array_rcv.is_equal(array_send));

        receiver.stop();
        EXPECT_TRUE(worker.stop_worker());
    }

    TEST_F(TestUdp, test_udp_sendrcv_connect)
    {
        sihd::util::ArrStr array_rcv(40);

        UdpSender sender("udp-sender");
        UdpReceiver receiver("udp-receiver");

        EXPECT_TRUE(sender.open_and_connect("127.0.0.1", 4242));
        EXPECT_TRUE(receiver.open_and_bind(IpAddr::get_localhost(4242)));

        EXPECT_TRUE(receiver.socket_opened());
        EXPECT_TRUE(receiver.socket().set_blocking(false));
        EXPECT_TRUE(sender.socket_opened());

        sihd::util::Handler<INetReceiver *> handler([&array_rcv] (INetReceiver *rcv)
        {
            rcv->receive(array_rcv);
            SIHD_LOG(debug, "Data received: " << array_rcv.to_string() << " - " << array_rcv.byte_size() << " bytes");
        });
        receiver.add_observer(&handler);

        const char helloworld[] = "hello world";
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

    TEST_F(TestUdp, test_udp_sendrcv_broadcast)
    {
        const char helloworld[] = "hello world";
        sihd::util::ArrStr array_rcv(40);

        UdpSender sender("udp-sender");
        EXPECT_TRUE(sender.open_socket());
        EXPECT_TRUE(sender.socket_opened());
        EXPECT_TRUE(sender.socket().set_broadcast(true));

        IpAddr any_ip(4242);
        UdpReceiver receiver("udp-receiver");
        EXPECT_TRUE(receiver.open_and_bind(any_ip));
        EXPECT_TRUE(receiver.socket_opened());

        IpAddr broadcast_ip("127.255.255.255", 4242, false);
        EXPECT_EQ(sender.send_to(broadcast_ip, helloworld, sizeof(helloworld)), (ssize_t)sizeof(helloworld));

        IpAddr iprcv;
        receiver.receive(iprcv, array_rcv);
        EXPECT_EQ(iprcv.get_first_ipv4(), "127.0.0.1");

        EXPECT_TRUE(array_rcv.is_equal(helloworld, sizeof(helloworld) - 1));
        EXPECT_EQ(array_rcv.size(), sizeof(helloworld));
    }

}