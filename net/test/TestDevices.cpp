#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

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

    Channel *rx = receiver->find_channel("rx");
    Channel *tx = sender->find_channel("tx");
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(receiver->is_running());

    ChannelWaiter waiter(rx);

    const char hello[] = "hello udp device";
    tx->write({hello, strlen(hello)});

    ASSERT_TRUE(waiter.wait_for(std::chrono::milliseconds(500)));

    EXPECT_EQ(rx->byte_size(), strlen(hello));
    EXPECT_EQ(memcmp(rx->data(), hello, strlen(hello)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(receiver->is_running());
}

TEST_F(TestDevices, test_tcp_client_device)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(server.set_reuseaddr(true));
    ASSERT_TRUE(server.bind(IpAddr::localhost(4301)));
    ASSERT_TRUE(server.listen(2));

    Core core;

    auto *client = core.add_child<DeviceTcpClient>("client");
    client->set_host("127.0.0.1");
    client->set_port(4301);
    client->set_poll_timeout(1);
    client->set_buffer_capacity(1024);
    client->set_connect_timeout(1000);

    ASSERT_TRUE(core.init());

    Channel *rx = client->find_channel("rx");
    Channel *tx = client->find_channel("tx");
    Channel *connected = client->find_channel("connected");
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);
    ASSERT_NE(connected, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), true);

    int accepted_fd = server.accept();
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);

    const char hello[] = "hello tcp device";
    tx->write({hello, strlen(hello)});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sihd::util::ArrChar recv_buf(64);
    ssize_t received = accepted.receive(recv_buf);
    EXPECT_EQ(received, (ssize_t)strlen(hello));
    EXPECT_EQ(memcmp(recv_buf.buf(), hello, strlen(hello)), 0);

    ChannelWaiter rx_waiter(rx);
    const char reply[] = "reply from server";
    EXPECT_EQ(accepted.send(reply), (ssize_t)strlen(reply));

    ASSERT_TRUE(rx_waiter.wait_for(std::chrono::milliseconds(500)));
    EXPECT_EQ(rx->byte_size(), strlen(reply));
    EXPECT_EQ(memcmp(rx->data(), reply, strlen(reply)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), false);

    accepted.close();
    server.close();
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

    Channel *rx = srv->find_channel("rx");
    Channel *tx = srv->find_channel("tx");
    Channel *client_count = srv->find_channel("client_count");
    ASSERT_NE(rx, nullptr);
    ASSERT_NE(tx, nullptr);
    ASSERT_NE(client_count, nullptr);

    ASSERT_TRUE(core.start());
    EXPECT_TRUE(srv->is_running());
    EXPECT_EQ(client_count->read<int32_t>(0), 0);

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(client.connect(IpAddr::localhost(4302)));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ChannelWaiter rx_waiter(rx);
    const char hello[] = "hello server device";
    EXPECT_EQ(client.send(hello), (ssize_t)strlen(hello));

    ASSERT_TRUE(rx_waiter.wait_for(std::chrono::milliseconds(500)));
    EXPECT_EQ(rx->byte_size(), strlen(hello));
    EXPECT_EQ(memcmp(rx->data(), hello, strlen(hello)), 0);

    EXPECT_EQ(client_count->read<int32_t>(0), 1);

    sihd::util::ArrChar recv_buf(64);
    const char broadcast[] = "broadcast msg";
    tx->write({broadcast, strlen(broadcast)});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client.set_blocking(false));
    ssize_t received = client.receive(recv_buf);
    EXPECT_EQ(received, (ssize_t)strlen(broadcast));
    EXPECT_EQ(memcmp(recv_buf.buf(), broadcast, strlen(broadcast)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(srv->is_running());

    client.close();
}

} // namespace test
