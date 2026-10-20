#include <gtest/gtest.h>

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/net/UdpSender.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>

using namespace sihd::util;

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
class TestUdp: public ::testing::Test
{
    protected:
        TestUdp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUdp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUdp, test_udp_receiver_run)
{
    sihd::util::ArrInt array_send = {1, 2, 3, 4, 5, 6};
    sihd::util::ArrInt array_rcv(array_send.size());
    UdpSender sender("udp-sender");
    UdpReceiver receiver("udp-receiver");

    EXPECT_TRUE(sender.open_and_connect({"127.0.0.1", 4242}).has_value());
    EXPECT_TRUE(receiver.open_and_bind({"127.0.0.1", 4242}).has_value());

    sihd::util::Synchronizer start_sync(2);
    ssize_t receive_ret = -1;
    sihd::util::Handler<INetReceiver *> handler([&receive_ret, &array_rcv, &start_sync](INetReceiver *rcv) {
        receive_ret = (ssize_t)rcv->receive(array_rcv).value_or(0);
        SIHD_LOG(debug, "Data received: {} - {} bytes", array_rcv.str(' '), array_rcv.byte_size());
        (void)start_sync.sync(std::chrono::milliseconds(500));
    });
    receiver.add_observer(&handler);
    receiver.set_poll_timeout(1);
    sihd::util::Worker worker([&receiver] { return receiver.start(); });
    SIHD_LOG(debug, "Starting receiver");
    EXPECT_TRUE(worker.start_sync_worker("receiver"));

    SIHD_LOG(debug, "Sending: {} ({} bytes)", array_send.str(','), array_send.byte_size());
    EXPECT_EQ(sender.send(array_send), (ssize_t)array_send.byte_size());

    EXPECT_TRUE(start_sync.sync(std::chrono::milliseconds(500)));

    EXPECT_TRUE(array_rcv.is_equal(array_send));

    receiver.stop();
    EXPECT_TRUE(worker.stop_worker());
}

TEST_F(TestUdp, test_udp_sendrcv_connect)
{
    sihd::util::ArrChar array_rcv(40);

    UdpSender sender("udp-sender");
    UdpReceiver receiver("udp-receiver");

    EXPECT_TRUE(sender.open_and_connect({"127.0.0.1", 4252}).has_value());
    EXPECT_TRUE(receiver.open_and_bind(IpAddr::localhost(4252)).has_value());

    EXPECT_TRUE(receiver.socket_opened());
    EXPECT_TRUE(receiver.socket().set_blocking(false));
    EXPECT_TRUE(sender.socket_opened());

    sihd::util::Handler<INetReceiver *> handler([&array_rcv](INetReceiver *rcv) {
        (void)rcv->receive(array_rcv);
        SIHD_LOG(debug, "Data received: {} - {} bytes", array_rcv.str(), array_rcv.byte_size());
    });
    receiver.add_observer(&handler);

    const char helloworld[] = "hello world";
    EXPECT_EQ(sender.send(helloworld), (ssize_t)strlen(helloworld));
    EXPECT_EQ(receiver.receive(array_rcv), (ssize_t)strlen(helloworld));

    EXPECT_TRUE(array_rcv.is_equal(helloworld));
    EXPECT_EQ(array_rcv.size(), strlen(helloworld));

    const char hello[] = "hello";
    EXPECT_EQ(sender.send(hello), (ssize_t)strlen(hello));
    EXPECT_TRUE(receiver.poll(10));

    EXPECT_TRUE(array_rcv.is_equal(hello));
    EXPECT_EQ(array_rcv.size(), strlen(hello));
}

TEST_F(TestUdp, test_udp_sendrcv_broadcast)
{
    const char helloworld[] = "hello world";
    sihd::util::ArrChar array_rcv(40);

    UdpSender sender("udp-sender");
    EXPECT_TRUE(sender.open_socket().has_value());
    EXPECT_TRUE(sender.socket_opened());
    EXPECT_TRUE(sender.socket().set_broadcast(true));

    IpAddr any_ip(4253);
    UdpReceiver receiver("udp-receiver");
    EXPECT_TRUE(receiver.open_and_bind(any_ip).has_value());
    EXPECT_TRUE(receiver.socket_opened());

    // send to broadcast ip
    EXPECT_EQ(sender.send_to({"127.255.255.255", 4253}, helloworld), (ssize_t)strlen(helloworld));

    IpAddr iprcv;
    (void)receiver.receive(iprcv, array_rcv);
    EXPECT_EQ(iprcv.str(), "127.0.0.1");

    EXPECT_TRUE(array_rcv.is_equal(helloworld));
    EXPECT_EQ(array_rcv.size(), strlen(helloworld));
}

TEST_F(TestUdp, test_udp_receiver_poll_timeout_and_unbound_errors)
{
    UdpReceiver receiver("udp-receiver");

    sihd::util::ArrChar array_rcv(64);
    auto unbound = receiver.receive(array_rcv);
    ASSERT_FALSE(unbound.has_value());
    EXPECT_EQ(unbound.error().code, sihd::util::ErrorCode::closed);

    EXPECT_FALSE(receiver.poll(20));

    ASSERT_TRUE(receiver.open_and_bind(IpAddr("127.0.0.1", 0)).has_value());
    const int port = receiver.socket().local_port().value();
    ASSERT_GT(port, 0);

    EXPECT_FALSE(receiver.poll(20));

    UdpSender sender("udp-sender");
    ASSERT_TRUE(sender.open_and_connect({"127.0.0.1", port}).has_value());
    const char msg[] = "wake";
    ASSERT_EQ(sender.send(msg), (ssize_t)strlen(msg));
    EXPECT_TRUE(receiver.poll(1000));

    EXPECT_TRUE(receiver.close().has_value());
    EXPECT_TRUE(sender.close().has_value());
}

TEST_F(TestUdp, test_udp_receiver_shares_its_port_unless_disabled)
{
    UdpReceiver first("udp-first");
    UdpReceiver second("udp-second");
    UdpReceiver exclusive("udp-exclusive");

    ASSERT_TRUE(first.open_and_bind(IpAddr::localhost(4243)).has_value());
    EXPECT_TRUE(second.open_and_bind(IpAddr::localhost(4243)).has_value());
    EXPECT_TRUE(exclusive.set_reuseaddr(false));
    EXPECT_FALSE(exclusive.open_and_bind(IpAddr::localhost(4243)).has_value());
}

TEST_F(TestUdp, test_udp_unix_roundtrip)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "AF_UNIX datagram is not supported by winsock";

    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine({dir.path(), "udp.sock"});

    UdpSender sender("udp-unix-sender");
    UdpReceiver receiver("udp-unix-receiver");

    ASSERT_TRUE(receiver.open_unix_and_bind(path).has_value());
    ASSERT_TRUE(sender.open_unix_and_connect(path).has_value());

    const char helloworld[] = "hello unix dgram";
    sihd::util::ArrChar array_rcv(64);
    ASSERT_EQ(sender.send(helloworld), (ssize_t)strlen(helloworld));
    EXPECT_EQ(receiver.receive(array_rcv), (ssize_t)strlen(helloworld));
    EXPECT_TRUE(array_rcv.is_equal(helloworld));

    EXPECT_TRUE(receiver.close().has_value());
    EXPECT_TRUE(sender.close().has_value());
}

} // namespace test
