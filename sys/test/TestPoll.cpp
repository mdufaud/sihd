#include <cstring>

#include <gtest/gtest.h>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/sys/Poll.hpp>

#if defined(__SIHD_WINDOWS__)
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <unistd.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

#if defined(__SIHD_WINDOWS__)

bool ensure_wsa()
{
    static bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return started;
}

// WSAPoll only polls sockets, so build a connected loopback TCP socket pair:
// fds[0] = read end (server), fds[1] = write end (client)
bool make_fd_pair(int fds[2])
{
    if (!ensure_wsa())
        return false;

    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        return false;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    int addrlen = sizeof(addr);
    if (::bind(listener, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR
        || ::getsockname(listener, (sockaddr *)&addr, &addrlen) == SOCKET_ERROR
        || ::listen(listener, 1) == SOCKET_ERROR)
    {
        ::closesocket(listener);
        return false;
    }

    SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET)
    {
        ::closesocket(listener);
        return false;
    }

    if (::connect(client, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        ::closesocket(listener);
        ::closesocket(client);
        return false;
    }

    SOCKET server = ::accept(listener, nullptr, nullptr);
    ::closesocket(listener);
    if (server == INVALID_SOCKET)
    {
        ::closesocket(client);
        return false;
    }

    // disable Nagle so small writes are readable within the poll timeout
    const int one = 1;
    ::setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    ::setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));

    fds[0] = (int)server;
    fds[1] = (int)client;
    return true;
}

ssize_t read_fd(int fd, void *buf, size_t count)
{
    return ::recv((SOCKET)fd, (char *)buf, (int)count, 0);
}

ssize_t write_fd(int fd, const void *buf, size_t count)
{
    return ::send((SOCKET)fd, (const char *)buf, (int)count, 0);
}

void close_fd(int fd)
{
    ::closesocket((SOCKET)fd);
}

#else

bool make_fd_pair(int fds[2])
{
    return pipe(fds) >= 0;
}

ssize_t read_fd(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

ssize_t write_fd(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

void close_fd(int fd)
{
    close(fd);
}

#endif

} // namespace

class TestPoll: public ::testing::Test
{
    protected:
        TestPoll() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPoll() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _timedout = 0;
            _read_count = 0;
            _write_count = 0;
            _to_close.clear();
        }

        virtual void TearDown()
        {
            for (int fd : _to_close)
            {
                close_fd(fd);
            }
        }

        int _timedout;
        int _read_count;
        int _write_count;
        std::vector<int> _to_close;
};

TEST_F(TestPoll, test_poll)
{
    Poll poll;
    poll.set_limit(2);

    int fd[2];
    EXPECT_TRUE(make_fd_pair(fd));
    _to_close.push_back(fd[0]);
    _to_close.push_back(fd[1]);
    poll.set_read_fd(fd[0]);
    poll.set_write_fd(fd[1]);

    char buffer[20];
    memset(buffer, 0, 20);

    Handler<Poll *> poll_handler([this, &buffer](Poll *poll) {
        SIHD_LOG(debug, "Polled");
        for (const PollEvent & event : poll->events())
        {
            int fd = event.fd;
            if (event.readable)
            {
                SIHD_LOG(debug, "Reading in fd: {}", fd);
                int ret = read_fd(fd, buffer, 20);
                buffer[ret] = 0;
                SIHD_LOG(debug, "Read {} bytes: '{}'", ret, buffer);
                _read_count += 1;
            }
            if (event.writable)
            {
                SIHD_LOG(debug, "Writing in fd: {}", fd);
                int ret = write_fd(fd, "hello world", 11);
                SIHD_LOG(debug, "Wrote {} bytes: 'hello world'", ret);
                _write_count += 1;
            }
        }
        _timedout += (int)poll->polling_timeout();
        SIHD_LOG(debug,
                 "Time spent in poll: {} microsec (timed out ? {})",
                 time::to_micro(poll->polling_time()),
                 poll->polling_timeout());
    });
    poll.add_observer(&poll_handler);

    constexpr int timeout_milliseconds = 10;

    // first write from writing end of pipe
    int res = poll.poll(timeout_milliseconds);
    // expect the write fd
    EXPECT_EQ(res, 1);
    // clearing the write fd so it does not write again
    poll.clear_fd(fd[1]);

    // nothing read yet
    EXPECT_EQ(strlen(buffer), 0u);
    EXPECT_EQ(_write_count, 1);
    EXPECT_EQ(_read_count, 0);
    EXPECT_EQ(_timedout, 0);

    // read from reading end of pipe
    res = poll.poll(timeout_milliseconds);
    // expect the read fd
    EXPECT_EQ(res, 1);

    // buffer filed
    EXPECT_EQ(strcmp(buffer, "hello world"), 0);
    EXPECT_EQ(_write_count, 1);
    EXPECT_EQ(_read_count, 1);
    EXPECT_EQ(_timedout, 0);

    // expect timeout
    res = poll.poll(timeout_milliseconds);
    EXPECT_EQ(res, 0);

    EXPECT_EQ(_write_count, 1);
    EXPECT_EQ(_read_count, 1);
    EXPECT_EQ(_timedout, 1);

    EXPECT_EQ(write_fd(fd[1], "hello world", 11), 11);
    res = poll.poll(timeout_milliseconds);
    EXPECT_EQ(res, 1);
    EXPECT_EQ(_write_count, 1);
    EXPECT_EQ(_read_count, 2);
    EXPECT_EQ(_timedout, 1);
}
} // namespace test
