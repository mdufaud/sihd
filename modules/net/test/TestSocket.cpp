#include <sys/stat.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_EMSCRIPTEN__)
# include <pthread.h>
# include <sys/socket.h>
# include <time.h>
# include <unistd.h>
#endif

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

    EXPECT_TRUE(socket_server.open(AF_INET6, SOCK_STREAM, IPPROTO_TCP).has_value());
    EXPECT_TRUE(socket_client.open(AF_INET6, SOCK_STREAM, IPPROTO_TCP).has_value());
    EXPECT_TRUE(socket_server.set_reuseaddr(true).has_value());

    SIHD_TRACE("Socket rcv: {}", socket_server.socket());
    SIHD_TRACE("Socket send: {}", socket_client.socket());

    IpAddr local_ipv6 = {"::1", 4200};
    const char buff[] = "hello world";
    size_t buff_len = strlen(buff);
    sihd::util::ArrChar byte_arr(buff_len + 1);

    EXPECT_TRUE(socket_server.bind(local_ipv6).has_value());
    EXPECT_TRUE(socket_server.listen(5).has_value());

    EXPECT_TRUE(socket_client.connect(local_ipv6).has_value());

    IpAddr addr;
    int accepted_socket = socket_server.accept(addr).value_or(-1);
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

    EXPECT_TRUE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_FALSE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_FALSE(socket_receive.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    EXPECT_TRUE(socket_receive.set_reuseaddr(true).has_value());
    EXPECT_TRUE(socket_receive.set_reuseaddr(true).has_value());

    EXPECT_TRUE(socket_send.open("ipv4", "datagram", "udp").has_value());

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

    EXPECT_TRUE(socket_receive.bind(local_ip).has_value());
    EXPECT_EQ(socket_send.send_to(local_ip, buff), (ssize_t)buff_len);
    EXPECT_EQ(socket_receive.receive_from(local_ip, byte_arr), (ssize_t)buff_len);
    EXPECT_EQ(strcmp(buff, byte_arr.data()), 0);
}

TEST_F(TestSocket, test_socket_datagram_connect)
{
    Socket socket_receive;
    Socket socket_send;

    EXPECT_TRUE(socket_receive.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(socket_receive.set_reuseaddr(true).has_value());

    EXPECT_TRUE(socket_send.open("ipv4", "datagram", "udp").has_value());

    IpAddr local_ip = {"127.0.0.1", 4200};
    const char buff[] = "hello world";
    size_t buff_len = strlen(buff);
    sihd::util::ArrChar byte_arr(buff_len + 1);

    EXPECT_TRUE(socket_receive.bind(local_ip).has_value());
    EXPECT_TRUE(socket_send.connect(local_ip).has_value());
    EXPECT_EQ(socket_send.send(buff), (ssize_t)buff_len);
    EXPECT_EQ(socket_receive.receive(byte_arr), (ssize_t)buff_len);
    EXPECT_EQ(strcmp(buff, byte_arr.data()), 0);
}

TEST_F(TestSocket, test_socket_options)
{
    Socket sock;
    EXPECT_TRUE(sock.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());

    EXPECT_TRUE(sock.set_keepalive(true).has_value());
    EXPECT_TRUE(sock.is_keepalive());
    EXPECT_TRUE(sock.set_keepalive(false).has_value());
    EXPECT_FALSE(sock.is_keepalive());

#ifdef SO_REUSEPORT
    EXPECT_TRUE(sock.set_reuseport(true));
    EXPECT_TRUE(sock.is_reuseport());
    EXPECT_TRUE(sock.set_reuseport(false));
    EXPECT_FALSE(sock.is_reuseport());
#endif

    EXPECT_TRUE(sock.set_rcvbuf(32768).has_value());
    EXPECT_GT(sock.get_rcvbuf(), 0);

    EXPECT_TRUE(sock.set_sndbuf(32768).has_value());
    EXPECT_GT(sock.get_sndbuf(), 0);
}

TEST_F(TestSocket, test_socket_multicast)
{
    Socket sock;
    EXPECT_TRUE(sock.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(sock.set_reuseaddr(true).has_value());

    IpAddr group("239.0.0.1");
    IpAddr bind_addr(4250);

    EXPECT_TRUE(sock.bind(bind_addr).has_value());
    EXPECT_TRUE(sock.join_multicast(group).has_value());
    EXPECT_TRUE(sock.set_multicast_ttl(2).has_value());
    EXPECT_TRUE(sock.set_multicast_loop(true).has_value());

    Socket sender;
    EXPECT_TRUE(sender.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(sender.set_multicast_loop(true).has_value());

    const char msg[] = "multicast";
    IpAddr dest("239.0.0.1", 4250);
    EXPECT_EQ(sender.send_to(dest, msg), (ssize_t)strlen(msg));

    sihd::sys::Poll poll(1);
    poll.set_read_fd(sock.socket());
    if (poll.poll(500) <= 0)
        GTEST_SKIP() << "Multicast loopback not available";
    sihd::util::ArrChar recv(32);
    ssize_t received = sock.receive(recv).value_or(0);
    EXPECT_EQ(received, (ssize_t)strlen(msg));
    EXPECT_EQ(strncmp(recv.data(), msg, strlen(msg)), 0);

    EXPECT_TRUE(sock.leave_multicast(group).has_value());
    EXPECT_TRUE(sock.close().has_value());
    EXPECT_TRUE(sender.close().has_value());
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestSocket, test_socket_unix_cleanup)
{
    std::string path = "/tmp/sihd_test_unix_cleanup.sock";

    {
        Socket server;
        EXPECT_TRUE(server.open(AF_UNIX, SOCK_STREAM, 0).has_value());
        EXPECT_TRUE(server.bind_unix(path).has_value());
        EXPECT_TRUE(server.listen(1).has_value());

        EXPECT_TRUE(sihd::sys::fs::exists(path));

        EXPECT_TRUE(server.close().has_value());
        EXPECT_FALSE(sihd::sys::fs::exists(path));
    }
}

TEST_F(TestSocket, test_socket_unix_abstract)
{
    // abstract namespace: the leading null byte is part of the address name
    const char raw_name[] = "\0sihd-net-test-abstract";
    const std::string_view name(raw_name, sizeof(raw_name) - 1);

    Socket server;
    ASSERT_TRUE(server.open(AF_UNIX, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(server.bind_unix(name).has_value());
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_UNIX, SOCK_STREAM, 0).has_value());
    ASSERT_TRUE(client.connect_unix(name).has_value());

    int accepted_fd = server.accept(1000).value_or(-1);
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);

    const sihd::util::ArrChar hello("abstract");
    EXPECT_TRUE(client.send_all(hello).has_value());
    sihd::util::ArrChar recv(64);
    EXPECT_EQ(accepted.receive(recv), (ssize_t)hello.size());
    EXPECT_EQ(strncmp(recv.data(), hello.data(), hello.size()), 0);

    EXPECT_EQ(Socket::unix_socket_peername(client.socket()), name);

    // abstract accepts the full sun_path, one byte more is refused
    std::string long_name(108, 'a');
    long_name[0] = '\0';
    Socket long_server;
    ASSERT_TRUE(long_server.open(AF_UNIX, SOCK_STREAM, 0).has_value());
    EXPECT_TRUE(long_server.bind_unix(long_name).has_value());
    std::string too_long = long_name + "b";
    Socket refused;
    ASSERT_TRUE(refused.open(AF_UNIX, SOCK_STREAM, 0).has_value());
    EXPECT_FALSE(refused.bind_unix(too_long).has_value());

    Socket empty_path;
    ASSERT_TRUE(empty_path.open(AF_UNIX, SOCK_STREAM, 0).has_value());
    EXPECT_FALSE(empty_path.bind_unix(std::string_view {}).has_value());
}
#endif

TEST_F(TestSocket, test_socket_move_construct_transfers_descriptor)
{
    Socket source(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TRUE(source.is_open());
    const int descriptor = source.socket();

    Socket moved(std::move(source));

    EXPECT_EQ(moved.socket(), descriptor);
    EXPECT_TRUE(moved.is_open());
    EXPECT_FALSE(source.is_open());
}

TEST_F(TestSocket, test_socket_move_assign_closes_previous)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 4210)).has_value());
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", 4210)).has_value());

    Socket replacement;
    ASSERT_TRUE(replacement.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());

    const int previous_fd = client.socket();
    ASSERT_GE(previous_fd, 0);

    client = std::move(replacement);

#if !defined(__SIHD_WINDOWS__)
    struct stat statbuf = {};
    const int fstat_ret = fstat(previous_fd, &statbuf);
    const int fstat_errno = errno;
    EXPECT_EQ(fstat_ret, -1);
    EXPECT_EQ(fstat_errno, EBADF);

    Socket self("ipv4", "stream", "tcp");
    const int self_fd = self.socket();
    Socket & self_ref = self;
    self = std::move(self_ref);
    EXPECT_EQ(self.socket(), self_fd);
#endif
}

TEST_F(TestSocket, test_socket_non_blocking_would_block)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 4211)).has_value());
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", 4211)).has_value());

    int accepted_fd = server.accept().value_or(-1);
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);
    ASSERT_TRUE(accepted.set_blocking(false).has_value());

    sihd::util::ArrChar recv(64);
    auto no_data = accepted.receive(recv);
    ASSERT_FALSE(no_data.has_value());
    EXPECT_TRUE(no_data.error().retryable());

    ASSERT_TRUE(accepted.set_blocking(true).has_value());
    const char msg[] = "hello";
    EXPECT_EQ(client.send(msg).value_or(0), strlen(msg));
    sihd::sys::Poll poller;
    poller.set_limit(1);
    poller.set_read_fd(accepted.socket());
    ASSERT_GT(poller.poll(500), 0);
    auto received = accepted.receive(recv);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received.value(), strlen(msg));
}

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_EMSCRIPTEN__)
namespace
{

void test_eintr_handler(int) {}

struct EintrArgs
{
        int fd;
};

void *test_eintr_thread(void *arg)
{
    auto *args = (EintrArgs *)arg;
    struct timespec fifty_ms = {0, 50 * 1000 * 1000};
    nanosleep(&fifty_ms, nullptr);
    kill(getpid(), SIGUSR1);
    struct timespec delay = {0, 120 * 1000 * 1000};
    nanosleep(&delay, nullptr);
    send(args->fd, "eintr", 5, 0);
    return nullptr;
}

} // namespace

TEST_F(TestSocket, test_socket_accept_would_block)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 4213)).has_value());
    ASSERT_TRUE(server.listen(1).has_value());
    ASSERT_TRUE(server.set_blocking(false).has_value());

    auto busy = server.accept();
    ASSERT_FALSE(busy.has_value());
    EXPECT_TRUE(busy.error().retryable());

    auto timed_out = server.accept(50);
    ASSERT_FALSE(timed_out.has_value());
    EXPECT_EQ(timed_out.error().code, sihd::util::ErrorCode::timeout);
}

TEST_F(TestSocket, test_socket_receive_eintr)
{
    // raw handler without SA_RESTART: the receive must be interrupted and retried
    struct sigaction sa = {};
    struct sigaction old_sa;
    sa.sa_handler = test_eintr_handler;
    ASSERT_EQ(sigaction(SIGUSR1, &sa, &old_sa), 0);
    // gtest shuffles: the handler must not leak to other tests
    sihd::util::Defer restore_handler([&] { sigaction(SIGUSR1, &old_sa, nullptr); });

    // block before creating the helper thread: it must not take the signal
    sigset_t blockset;
    sigset_t old_mask;
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGUSR1);
    ASSERT_EQ(pthread_sigmask(SIG_BLOCK, &blockset, &old_mask), 0);
    sihd::util::Defer restore_mask([&] { pthread_sigmask(SIG_SETMASK, &old_mask, nullptr); });

    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 4212)).has_value());
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", 4212)).has_value());
    int accepted_fd = server.accept().value_or(-1);
    ASSERT_GE(accepted_fd, 0);
    Socket accepted(accepted_fd);

    EintrArgs args = {.fd = client.socket()};
    pthread_t thread;
    ASSERT_EQ(pthread_create(&thread, nullptr, test_eintr_thread, &args), 0);

    pthread_sigmask(SIG_UNBLOCK, &blockset, nullptr);

    sihd::util::ArrChar recv(64);
    EXPECT_EQ(accepted.receive(recv), 5);
    EXPECT_EQ(strncmp(recv.data(), "eintr", 5), 0);

    pthread_join(thread, nullptr);
}
#endif

TEST_F(TestSocket, test_socket_options_extended)
{
    Socket sock;
    ASSERT_TRUE(sock.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());

    EXPECT_TRUE(sock.set_tcp_nodelay(true).has_value());
    EXPECT_TRUE(sock.is_tcp_nodelay());
    EXPECT_TRUE(sock.set_tcp_nodelay(false).has_value());
#if !defined(__SIHD_WINDOWS__)
    EXPECT_FALSE(sock.is_tcp_nodelay());
#endif

    EXPECT_TRUE(sock.set_ttl(64).has_value());

    Socket udp;
    ASSERT_TRUE(udp.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(udp.set_broadcast(true).has_value());
    EXPECT_TRUE(udp.is_broadcast());
    EXPECT_TRUE(udp.set_broadcast(false).has_value());
#if !defined(__SIHD_WINDOWS__)
    EXPECT_FALSE(udp.is_broadcast());
#endif
}

#if defined(__SIHD_LINUX__)
TEST_F(TestSocket, test_socket_bind_to_device)
{
    Socket sock;
    ASSERT_TRUE(sock.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(sock.bind_to_device("lo").has_value());
    EXPECT_FALSE(sock.bind_to_device("no-such-iface-sihd").has_value());
}
#endif

TEST_F(TestSocket, test_socket_recv_timeout_expires)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 0)).has_value());
    const int port = server.local_port();
    ASSERT_GT(port, 0);
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", port)).has_value());
    Socket accepted(server.accept().value_or(-1));
    ASSERT_TRUE(accepted.is_open());

    ASSERT_TRUE(accepted.set_recv_timeout(50).has_value());
    sihd::util::ArrChar recv(64);
    const auto start = std::chrono::steady_clock::now();
    auto res = accepted.receive(recv);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                               - start);
    ASSERT_FALSE(res.has_value());
    EXPECT_GE(elapsed.count(), 25);
    EXPECT_LT(elapsed.count(), 2000);
}

TEST_F(TestSocket, test_socket_send_all_non_blocking_would_block)
{
    Socket server;
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(server.set_reuseaddr(true).has_value());
    ASSERT_TRUE(server.bind(IpAddr("127.0.0.1", 0)).has_value());
    const int port = server.local_port();
    ASSERT_GT(port, 0);
    ASSERT_TRUE(server.listen(1).has_value());

    Socket client;
    ASSERT_TRUE(client.open(AF_INET, SOCK_STREAM, IPPROTO_TCP).has_value());
    ASSERT_TRUE(client.connect(IpAddr("127.0.0.1", port)).has_value());
    Socket accepted(server.accept().value_or(-1));
    ASSERT_TRUE(accepted.is_open());

    ASSERT_TRUE(client.set_sndbuf(2048).has_value());
    ASSERT_TRUE(client.set_blocking(false).has_value());

    const std::string payload(256 * 1024, 'x');
    auto res = client.send_all({payload.data(), payload.size()});
    // the socket buffer size is a hint the platform may ignore: keep pouring until it refuses
    for (int i = 0; res.has_value() && i < 64; ++i)
        res = client.send_all({payload.data(), payload.size()});
    ASSERT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().retryable());

    EXPECT_TRUE(client.close().has_value());
    EXPECT_TRUE(accepted.close().has_value());
    EXPECT_TRUE(server.close().has_value());
}

TEST_F(TestSocket, test_socket_send_all_to_datagram_refused)
{
    Socket receiver;
    ASSERT_TRUE(receiver.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    ASSERT_TRUE(receiver.set_reuseaddr(true).has_value());
    ASSERT_TRUE(receiver.bind(IpAddr("127.0.0.1", 0)).has_value());
    const int port = receiver.local_port();
    ASSERT_GT(port, 0);

    Socket sender;
    ASSERT_TRUE(sender.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    ASSERT_TRUE(sender.set_sndbuf(2048).has_value());
    ASSERT_TRUE(sender.set_blocking(false).has_value());

    const std::string payload(64 * 1024, 'y');
    auto res = sender.send_all_to(IpAddr("127.0.0.1", port), {payload.data(), payload.size()});
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, sihd::util::ErrorCode::overflow);

    EXPECT_TRUE(sender.close().has_value());
    EXPECT_TRUE(receiver.close().has_value());
}

TEST_F(TestSocket, test_socket_send_receive_after_close)
{
    Socket sock;
    ASSERT_TRUE(sock.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP).has_value());
    EXPECT_TRUE(sock.close().has_value());

    auto sent = sock.send("hello");
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error().code, sihd::util::ErrorCode::closed);

    sihd::util::ArrChar recv(16);
    auto received = sock.receive(recv);
    ASSERT_FALSE(received.has_value());
    EXPECT_EQ(received.error().code, sihd::util::ErrorCode::closed);
}

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_EMSCRIPTEN__)
TEST_F(TestSocket, test_socket_unix_datagram_roundtrip)
{
    const std::string path = "/tmp/sihd_test_unix_dgram_" + std::to_string(getpid()) + ".sock";
    const std::string sender_path = path + ".sender";
    sihd::util::Defer cleanup([&] {
        (void)sihd::sys::fs::remove_file(path);
        (void)sihd::sys::fs::remove_file(sender_path);
    });

    Socket receiver;
    ASSERT_TRUE(receiver.open(AF_UNIX, SOCK_DGRAM, 0).has_value());
    ASSERT_TRUE(receiver.bind_unix(path).has_value());

    Socket sender;
    ASSERT_TRUE(sender.open(AF_UNIX, SOCK_DGRAM, 0).has_value());
    const char msg[] = "unix dgram";
    ASSERT_EQ(sender.send_to_unix(path, msg), (ssize_t)strlen(msg));

    std::string from;
    sihd::util::ArrChar recv(64);
    EXPECT_EQ(receiver.receive_from_unix(from, recv), (ssize_t)strlen(msg));
    EXPECT_EQ(strncmp(recv.data(), msg, strlen(msg)), 0);

    Socket bound_sender;
    ASSERT_TRUE(bound_sender.open(AF_UNIX, SOCK_DGRAM, 0).has_value());
    ASSERT_TRUE(bound_sender.bind_unix(sender_path).has_value());
    ASSERT_EQ(bound_sender.send_to_unix(path, msg), (ssize_t)strlen(msg));
    EXPECT_EQ(receiver.receive_from_unix(from, recv), (ssize_t)strlen(msg));
    EXPECT_EQ(from, sender_path);

    const sihd::util::ArrChar payload("all the bytes");
    EXPECT_TRUE(bound_sender.send_all_to_unix(path, payload).has_value());
    sihd::util::ArrChar recv2(64);
    EXPECT_EQ(receiver.receive_from_unix(from, recv2), (ssize_t)payload.size());
    EXPECT_EQ(strncmp(recv2.data(), payload.data(), payload.size()), 0);
}
#endif

} // namespace test
