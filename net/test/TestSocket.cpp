#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
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
        TestSocket() { sihd::util::LoggerManager::stream(); }

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
    EXPECT_TRUE(addr.is_ipv6());

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

TEST_F(TestSocket, test_socket_options)
{
    Socket sock;
    EXPECT_TRUE(sock.open(AF_INET, SOCK_STREAM, IPPROTO_TCP));

    EXPECT_TRUE(sock.set_keepalive(true));
    EXPECT_TRUE(sock.is_keepalive());
    EXPECT_TRUE(sock.set_keepalive(false));
    EXPECT_FALSE(sock.is_keepalive());

#ifdef SO_REUSEPORT
    EXPECT_TRUE(sock.set_reuseport(true));
    EXPECT_TRUE(sock.is_reuseport());
    EXPECT_TRUE(sock.set_reuseport(false));
    EXPECT_FALSE(sock.is_reuseport());
#endif

    EXPECT_TRUE(sock.set_rcvbuf(32768));
    EXPECT_GT(sock.get_rcvbuf(), 0);

    EXPECT_TRUE(sock.set_sndbuf(32768));
    EXPECT_GT(sock.get_sndbuf(), 0);
}

TEST_F(TestSocket, test_socket_multicast)
{
    Socket sock;
    EXPECT_TRUE(sock.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    EXPECT_TRUE(sock.set_reuseaddr(true));

    IpAddr group("239.0.0.1");
    IpAddr bind_addr(4250);

    EXPECT_TRUE(sock.bind(bind_addr));
    EXPECT_TRUE(sock.join_multicast(group));
    EXPECT_TRUE(sock.set_multicast_ttl(2));
    EXPECT_TRUE(sock.set_multicast_loop(true));

    Socket sender;
    EXPECT_TRUE(sender.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    EXPECT_TRUE(sender.set_multicast_loop(true));

    const char msg[] = "multicast";
    IpAddr dest("239.0.0.1", 4250);
    EXPECT_EQ(sender.send_to(dest, msg), (ssize_t)strlen(msg));

    sihd::sys::Poll poll(1);
    poll.set_read_fd(sock.socket());
    if (poll.poll(500) <= 0)
        GTEST_SKIP() << "Multicast loopback not available";
    sihd::util::ArrChar recv(32);
    ssize_t received = sock.receive(recv);
    EXPECT_EQ(received, (ssize_t)strlen(msg));
    EXPECT_EQ(strncmp(recv.data(), msg, strlen(msg)), 0);

    EXPECT_TRUE(sock.leave_multicast(group));
    EXPECT_TRUE(sock.close());
    EXPECT_TRUE(sender.close());
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestSocket, test_socket_unix_cleanup)
{
    std::string path = "/tmp/sihd_test_unix_cleanup.sock";

    {
        Socket server;
        EXPECT_TRUE(server.open(AF_UNIX, SOCK_STREAM, 0));
        EXPECT_TRUE(server.bind_unix(path));
        EXPECT_TRUE(server.listen(1));

        EXPECT_TRUE(sihd::sys::fs::exists(path));

        EXPECT_TRUE(server.close());
        EXPECT_FALSE(sihd::sys::fs::exists(path));
    }
}
#endif

} // namespace test
