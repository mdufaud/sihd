#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

#include "listener/make_listener.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::net;

class TestDevices: public ::testing::Test
{
    protected:
        TestDevices() { sihd::util::LoggerManager::stream(); }

        virtual ~TestDevices() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevices, test_udp_devices)
{
    Core core;

    auto *sender = core.add_child<DeviceUdpSender>("sender");
    auto *receiver = core.add_child<DeviceUdpReceiver>("receiver");

    sender->set_host("127.0.0.1");
    sender->set_port(4300);
    receiver->set_host("127.0.0.1");
    receiver->set_port(4300);
    receiver->set_poll_timeout(1);
    receiver->set_buffer_capacity(1024);

    ASSERT_TRUE(core.init());

    Channel *rx = receiver->find_channel("rx").value_or(nullptr);
    Channel *tx = sender->find_channel("tx").value_or(nullptr);
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(receiver->is_running());

    ChannelWaiter waiter(rx);

    const char hello[] = "hello udp device";
    tx->write({hello, strlen(hello)});

    // prev: the receiver device may have delivered rx before this call
    ASSERT_TRUE(waiter.prev_wait_for(std::chrono::seconds(5)));

    EXPECT_EQ(rx->byte_size(), strlen(hello));
    EXPECT_EQ(memcmp(rx->data(), hello, strlen(hello)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(receiver->is_running());
}

TEST_F(TestDevices, test_tcp_client_device)
{
    Socket server;
    make_listener(server, IpAddr::localhost(4301));

    Core core;

    auto *client = core.add_child<DeviceTcpClient>("client");
    client->set_host("127.0.0.1");
    client->set_port(4301);
    client->set_poll_timeout(1);
    client->set_buffer_capacity(1024);
    client->set_connect_timeout(1000);

    ASSERT_TRUE(core.init());

    Channel *rx = client->find_channel("rx").value_or(nullptr);
    Channel *tx = client->find_channel("tx").value_or(nullptr);
    Channel *connected = client->find_channel("connected").value_or(nullptr);
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);
    ASSERT_NE(connected, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), true);

    int accepted_fd = server.accept().value_or(-1);
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);

    const char hello[] = "hello tcp device";
    tx->write({hello, strlen(hello)});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sihd::util::ArrChar recv_buf(64);
    ssize_t received = accepted.receive(recv_buf).value_or(0);
    EXPECT_EQ(received, (ssize_t)strlen(hello));
    EXPECT_EQ(memcmp(recv_buf.buf(), hello, strlen(hello)), 0);

    ChannelWaiter rx_waiter(rx);
    const char reply[] = "reply from server";
    EXPECT_EQ(accepted.send(reply), (ssize_t)strlen(reply));

    ASSERT_TRUE(rx_waiter.wait_for_nb(std::chrono::milliseconds(500), 1));
    EXPECT_EQ(rx->byte_size(), strlen(reply));
    EXPECT_EQ(memcmp(rx->data(), reply, strlen(reply)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), false);

    (void)accepted.close();
    (void)server.close();
}

TEST_F(TestDevices, test_tcp_server_device)
{
    Core core;

    auto *srv = core.add_child<DeviceTcpServer>("server");
    srv->set_host("127.0.0.1");
    srv->set_port(4302);
    srv->set_poll_timeout(1);
    srv->set_queue_size(2);
    srv->set_max_clients(4);
    srv->set_buffer_capacity(1024);

    ASSERT_TRUE(core.init());

    Channel *rx = srv->find_channel("rx").value_or(nullptr);
    Channel *tx = srv->find_channel("tx").value_or(nullptr);
    Channel *client_count = srv->find_channel("client_count").value_or(nullptr);
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);
    ASSERT_NE(client_count, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(srv->is_running());
    EXPECT_EQ(client_count->read<int32_t>(0), 0);

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(client.connect(IpAddr::localhost(4302)).has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ChannelWaiter rx_waiter(rx);
    const char hello[] = "hello server device";
    EXPECT_EQ(client.send(hello), (ssize_t)strlen(hello));

    ASSERT_TRUE(rx_waiter.wait_for_nb(std::chrono::milliseconds(500), 1));
    EXPECT_EQ(rx->byte_size(), strlen(hello));
    EXPECT_EQ(memcmp(rx->data(), hello, strlen(hello)), 0);

    EXPECT_EQ(client_count->read<int32_t>(0), 1);

    sihd::util::ArrChar recv_buf(64);
    const char broadcast[] = "broadcast msg";
    tx->write({broadcast, strlen(broadcast)});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client.set_blocking(false).has_value());
    ssize_t received = client.receive(recv_buf).value_or(0);
    EXPECT_EQ(received, (ssize_t)strlen(broadcast));
    EXPECT_EQ(memcmp(recv_buf.buf(), broadcast, strlen(broadcast)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(srv->is_running());

    (void)client.close();
}

TEST_F(TestDevices, test_tcp_client_device_reconnect)
{
    Socket server;
    make_listener(server, IpAddr::localhost(4303));

    Core core;

    auto *client = core.add_child<DeviceTcpClient>("client");
    client->set_host("127.0.0.1");
    client->set_port(4303);
    client->set_poll_timeout(1);
    client->set_buffer_capacity(1024);
    client->set_connect_timeout(200);
    client->set_reconnect_interval(50);

    ASSERT_TRUE(core.init());

    Channel *rx = client->find_channel("rx").value_or(nullptr);
    Channel *connected = client->find_channel("connected").value_or(nullptr);
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(connected, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_EQ(connected->read<bool>(0), true);

    int accepted_fd = server.accept().value_or(-1);
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);

    (void)accepted.close();
    (void)server.close();

    // qemu emulated targets run the reconnect cycles far slower than native
    wait_for([&] { return connected->read<bool>(0) == false; });
    EXPECT_EQ(connected->read<bool>(0), false);

    Socket server2;
    make_listener(server2, IpAddr::localhost(4303));

    // the waiter must exist first: qemu delivery wins the race
    ChannelWaiter rx_waiter(rx);
    wait_for([&] { return connected->read<bool>(0) == true; });
    EXPECT_EQ(connected->read<bool>(0), true);
    int fd2 = server2.accept(2000).value_or(-1);
    ASSERT_GE(fd2, 0);
    Socket accepted2(fd2);
    const char hello[] = "reconnected";
    EXPECT_EQ(accepted2.send(hello), (ssize_t)strlen(hello));
    // prev: the device may have delivered rx before this call
    EXPECT_TRUE(rx_waiter.prev_wait_for(std::chrono::seconds(5)));
    EXPECT_EQ(memcmp(rx->data(), hello, strlen(hello)), 0);

    Channel *tx = client->find_channel("tx").value_or(nullptr);
    ASSERT_NE(tx, nullptr);
    const char tx_msg[] = "tx after reconnect";
    tx->write({tx_msg, strlen(tx_msg)});
    char tx_buf[64] = {0};
    ssize_t tx_received = 0;
    ASSERT_TRUE(accepted2.set_blocking(false).has_value());
    wait_for([&] {
        tx_received = accepted2.receive(tx_buf, sizeof(tx_buf)).value_or(0);
        return tx_received > 0;
    });
    EXPECT_EQ(tx_received, (ssize_t)strlen(tx_msg));
    EXPECT_EQ(memcmp(tx_buf, tx_msg, strlen(tx_msg)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), false);

    (void)server2.close();
}

} // namespace test
