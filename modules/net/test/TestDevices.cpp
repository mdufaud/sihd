#include <array>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/net/ClientFrame.hpp>
#include <sihd/net/DevTcpClient.hpp>
#include <sihd/net/DevTcpServer.hpp>
#include <sihd/net/DevUdpReceiver.hpp>
#include <sihd/net/DevUdpSender.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

#include "listener/make_listener.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::net;
using namespace sihd::util;

class TestDevices: public ::testing::Test
{
    protected:
        TestDevices() { LoggerManager::stream(); }

        virtual ~TestDevices() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        int free_port()
        {
            Socket probe;
            EXPECT_TRUE(probe.open(AF_INET, SOCK_STREAM, 0).has_value());
            EXPECT_TRUE(probe.bind(IpAddr::localhost(0)).has_value());
            return probe.local_port().value_or(-1);
        }

        Socket connect_client(int port)
        {
            Socket client;
            EXPECT_TRUE(client.open(AF_INET, SOCK_STREAM, 0).has_value());
            EXPECT_TRUE(client.connect(IpAddr::localhost(port)).has_value());
            EXPECT_TRUE(client.set_recv_timeout(2000).has_value());
            return client;
        }

        std::string receive_text(Socket & client)
        {
            std::array<char, 64> buf {};
            const size_t received = client.receive(buf.data(), buf.size()).value_or(0);
            return std::string(buf.data(), received);
        }
};

TEST_F(TestDevices, test_udp_devices)
{
    Core core;

    auto *sender = core.add_child<DevUdpSender>("sender");
    auto *receiver = core.add_child<DevUdpReceiver>("receiver");

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

    auto *client = core.add_child<DevTcpClient>("client");
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

    ArrChar recv_buf(64);
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

    auto *srv = core.add_child<DevTcpServer>("server");
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

    ArrChar recv_buf(64);
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

    auto *client = core.add_child<DevTcpClient>("client");
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
    std::array<char, 64> tx_buf {};
    ssize_t tx_received = 0;
    ASSERT_TRUE(accepted2.set_blocking(false).has_value());
    wait_for([&] {
        tx_received = accepted2.receive(tx_buf.data(), tx_buf.size()).value_or(0);
        return tx_received > 0;
    });
    EXPECT_EQ(tx_received, (ssize_t)strlen(tx_msg));
    EXPECT_EQ(memcmp(tx_buf.data(), tx_msg, strlen(tx_msg)), 0);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(client->is_running());
    EXPECT_EQ(connected->read<bool>(0), false);

    (void)server2.close();
}

TEST_F(TestDevices, test_tcp_server_device_addresses_one_client)
{
    Core core;
    auto *srv = core.add_child<DevTcpServer>("server");
    srv->set_host("127.0.0.1");
    const int port = this->free_port();
    srv->set_port(port);
    srv->set_poll_timeout(1);
    srv->set_buffer_capacity(1024);
    ASSERT_TRUE(core.init());

    Channel *rx_from = srv->find_channel("rx_from").value_or(nullptr);
    Channel *tx_to = srv->find_channel("tx_to").value_or(nullptr);
    Channel *connected = srv->find_channel("connected").value_or(nullptr);
    ASSERT_NE(rx_from, nullptr);
    ASSERT_NE(tx_to, nullptr);
    ASSERT_NE(connected, nullptr);
    ASSERT_TRUE(core.start());

    ChannelWaiter connected_waiter(connected);
    Socket client_a = this->connect_client(port);
    ASSERT_TRUE(connected_waiter.prev_wait_for(std::chrono::seconds(5)));
    const uint64_t id_a = connected->read<uint64_t>(0);
    Socket client_b = this->connect_client(port);
    ASSERT_TRUE(connected_waiter.prev_wait_for(std::chrono::seconds(5)));
    const uint64_t id_b = connected->read<uint64_t>(0);
    EXPECT_NE(id_a, id_b);

    ChannelWaiter rx_from_waiter(rx_from);
    const char from_a[] = "from a";
    EXPECT_EQ(client_a.send(from_a), (ssize_t)strlen(from_a));
    ASSERT_TRUE(rx_from_waiter.prev_wait_for(std::chrono::seconds(5)));
    auto frame = ClientFrame::decode(ArrByteView(*rx_from->array()));
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->client_id(), id_a);
    EXPECT_EQ(frame->payload().str(), "from a");

    ASSERT_TRUE(tx_to->write(ClientFrame(id_b, "to b").encode()));
    EXPECT_EQ(this->receive_text(client_b), "to b");

    ASSERT_TRUE(client_a.set_blocking(false).has_value());
    std::array<char, 16> a_buf {};
    EXPECT_FALSE(client_a.receive(a_buf.data(), a_buf.size()).has_value());

    ASSERT_TRUE(core.stop());
}

TEST_F(TestDevices, test_tcp_server_device_drops_writes_for_gone_clients)
{
    Core core;
    auto *srv = core.add_child<DevTcpServer>("server");
    srv->set_host("127.0.0.1");
    const int port = this->free_port();
    srv->set_port(port);
    srv->set_poll_timeout(1);
    srv->set_buffer_capacity(1024);
    ASSERT_TRUE(core.init());

    Channel *tx_to = srv->find_channel("tx_to").value_or(nullptr);
    Channel *connected = srv->find_channel("connected").value_or(nullptr);
    Channel *disconnected = srv->find_channel("disconnected").value_or(nullptr);
    ASSERT_NE(tx_to, nullptr);
    ASSERT_NE(connected, nullptr);
    ASSERT_NE(disconnected, nullptr);
    ASSERT_TRUE(core.start());

    ChannelWaiter connected_waiter(connected);
    ChannelWaiter disconnected_waiter(disconnected);
    Socket gone = this->connect_client(port);
    ASSERT_TRUE(connected_waiter.prev_wait_for(std::chrono::seconds(5)));
    const uint64_t gone_id = connected->read<uint64_t>(0);
    (void)gone.close();
    ASSERT_TRUE(disconnected_waiter.prev_wait_for(std::chrono::seconds(5)));
    EXPECT_EQ(disconnected->read<uint64_t>(0), gone_id);

    Socket next = this->connect_client(port);
    ASSERT_TRUE(connected_waiter.prev_wait_for(std::chrono::seconds(5)));
    const uint64_t next_id = connected->read<uint64_t>(0);
    EXPECT_NE(next_id, gone_id);

    ASSERT_TRUE(tx_to->write(ClientFrame(gone_id, "late").encode()));
    ASSERT_TRUE(tx_to->write(ClientFrame(next_id, "fresh").encode()));
    // tcp keeps the order: the late write would come first if it had been sent
    EXPECT_EQ(this->receive_text(next), "fresh");

    ASSERT_TRUE(core.stop());
}

} // namespace test
