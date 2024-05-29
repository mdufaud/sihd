#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/net/Socket.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
class TestSocket: public ::testing::Test
{
    protected:
        TestSocket() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSocket() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSocket, test_socket_stream_client_server)
{
    Socket socket_server;
    Socket socket_client;

    EXPECT_TRUE(socket_server.open(AF_INET6, SOCK_STREAM, IPPROTO_TCP));
    EXPECT_TRUE(socket_client.open(AF_INET6, SOCK_STREAM, IPPROTO_TCP));
    EXPECT_TRUE(socket_server.set_reuseaddr(true));

    SIHD_TRACE("Socket rcv: {}", socket_server.socket());
    SIHD_TRACE("Socket send: {}", socket_client.socket());

    IpAddr local_ipv6 = {"::1", 4200};
    const char buff[] = "hello world";
    size_t buff_len = strlen(buff);
    sihd::util::ArrChar byte_arr(buff_len + 1);

    EXPECT_TRUE(socket_server.bind(local_ipv6));
    EXPECT_TRUE(socket_server.listen(5));

    EXPECT_TRUE(socket_client.connect(local_ipv6));

    IpAddr addr;
    int accepted_socket = socket_server.accept(addr);
    EXPECT_TRUE(accepted_socket >= 0);
    EXPECT_EQ(addr.first_ipv4_str(), "");
    EXPECT_EQ(addr.first_ipv6_str(), "::1");

    Socket connected_socket(accepted_socket);
    EXPECT_EQ(connected_socket.domain(), AF_INET6);
    EXPECT_EQ(connected_socket.type(), SOCK_STREAM);
    EXPECT_EQ(connected_socket.protocol(), IPPROTO_TCP);

    EXPECT_EQ(socket_client.send(buff), (ssize_t)buff_len);
    EXPECT_EQ(connected_socket.receive(byte_arr), (ssize_t)buff_len);
    EXPECT_EQ(strcmp(buff, byte_arr.data()), 0);
}

TEST_F(TestSocket, test_socket_datagram_no_connect)
{
    Socket socket_receive;
    Socket socket_send;

    EXPECT_FALSE(socket_receive.is_open());
    EXPECT_EQ(socket_receive.socket(), -1);

    EXPECT_TRUE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    EXPECT_FALSE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    EXPECT_FALSE(socket_receive.open(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    EXPECT_TRUE(socket_receive.set_reuseaddr(true));
    EXPECT_TRUE(socket_receive.set_reuseaddr(true));

    EXPECT_TRUE(socket_send.open("ipv4", "datagram", "udp"));

    EXPECT_EQ(socket_receive.domain(), AF_INET);
    EXPECT_EQ(socket_receive.type(), SOCK_DGRAM);
    EXPECT_EQ(socket_receive.protocol(), IPPROTO_UDP);

    EXPECT_EQ(socket_send.domain(), AF_INET);
    EXPECT_EQ(socket_send.type(), SOCK_DGRAM);
    EXPECT_EQ(socket_send.protocol(), IPPROTO_UDP);

    IpAddr local_ip = {"127.0.0.1", 4200};
    const char buff[] = "hello world";
    size_t buff_len = strlen(buff);
    sihd::util::ArrChar byte_arr(buff_len + 1);

    EXPECT_TRUE(socket_receive.bind(local_ip));
    EXPECT_EQ(socket_send.send_to(local_ip, buff), (ssize_t)buff_len);
    EXPECT_EQ(socket_receive.receive_from(local_ip, byte_arr), (ssize_t)buff_len);
    EXPECT_EQ(strcmp(buff, byte_arr.data()), 0);
}

TEST_F(TestSocket, test_socket_datagram_connect)
{
    Socket socket_receive;
    Socket socket_send;

    EXPECT_TRUE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    EXPECT_TRUE(socket_receive.set_reuseaddr(true));

    EXPECT_TRUE(socket_send.open("ipv4", "datagram", "udp"));

    IpAddr local_ip = {"127.0.0.1", 4200};
    const char buff[] = "hello world";
    size_t buff_len = strlen(buff);
    sihd::util::ArrChar byte_arr(buff_len + 1);

    EXPECT_TRUE(socket_receive.bind(local_ip));
    EXPECT_TRUE(socket_send.connect(local_ip));
    EXPECT_EQ(socket_send.send(buff), (ssize_t)buff_len);
    EXPECT_EQ(socket_receive.receive(byte_arr), (ssize_t)buff_len);
    EXPECT_EQ(strcmp(buff, byte_arr.data()), 0);
}

} // namespace test
