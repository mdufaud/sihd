#include <cstring>

#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

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
    Poll empty(2);
    EXPECT_FALSE(empty.rm_read_fd(42));
    EXPECT_FALSE(empty.rm_write_fd(42));
    Poll poll;
    poll.set_limit(2);

    int fd[2];
    EXPECT_TRUE(make_fd_pair(fd));
    _to_close.push_back(fd[0]);
    _to_close.push_back(fd[1]);
    poll.set_read_fd(fd[0]);
    poll.set_write_fd(fd[1]);
    EXPECT_EQ(poll.read_fds_size(), 1u);
    EXPECT_EQ(poll.write_fds_size(), 1u);
    EXPECT_FALSE(poll.rm_read_fd(-2));
    EXPECT_FALSE(poll.rm_write_fd(-2));

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
    EXPECT_FALSE(poll.rm_read_fd(-1));
    EXPECT_FALSE(poll.rm_write_fd(-1));

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
