#include <array>
#include <atomic>

#include <gtest/gtest.h>

#include <sihd/sys/EventPoll.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestEventPoll: public ::testing::Test
{
    protected:
        TestEventPoll() { LoggerManager::stream(); }

        virtual ~TestEventPoll() { LoggerManager::clear_loggers(); }

        virtual void TearDown()
        {
            for (int fd : _to_close)
            {
                close_fd(fd);
            }
        }

        std::array<int, 2> fd_pair()
        {
            std::array<int, 2> fds = {-1, -1};
            EXPECT_TRUE(make_fd_pair(fds));
            _to_close.push_back(fds[0]);
            _to_close.push_back(fds[1]);
            return fds;
        }

        void start_service(EventPoll & poll)
        {
            _worker.set_method([&poll] { return poll.start(); });
            ASSERT_TRUE(_worker.start_worker("test-eventpoll"));
            ASSERT_TRUE(poll.wait_ready(Duration(time::sec(1))));
        }

        void stop_service(EventPoll & poll)
        {
            EXPECT_TRUE(poll.stop());
            EXPECT_TRUE(_worker.stop_worker());
        }

        std::vector<int> _to_close;
        Worker _worker;
};

TEST_F(TestEventPoll, test_eventpoll_reports_a_readable_fd)
{
    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;
    ASSERT_TRUE(poll.set_read_fd(fd[0]));
    ASSERT_EQ(write_fd(fd[1], "x", 1), 1);

    EXPECT_EQ(poll.poll(1000), 1);

    ASSERT_EQ(poll.events().size(), 1u);
    EXPECT_EQ(poll.events()[0].fd, fd[0]);
    EXPECT_TRUE(poll.events()[0].readable);
    EXPECT_FALSE(poll.events()[0].writable);
}

TEST_F(TestEventPoll, test_eventpoll_times_out)
{
    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;
    ASSERT_TRUE(poll.set_read_fd(fd[0]));

    EXPECT_EQ(poll.poll(10), 0);

    EXPECT_TRUE(poll.polling_timeout());
    EXPECT_TRUE(poll.events().empty());
}

TEST_F(TestEventPoll, test_eventpoll_counts_and_removes_interests)
{
    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;

    ASSERT_TRUE(poll.set_read_fd(fd[0]));
    ASSERT_TRUE(poll.set_write_fd(fd[0]));
    EXPECT_EQ(poll.fds_size(), 1u);
    EXPECT_EQ(poll.read_fds_size(), 1u);
    EXPECT_EQ(poll.write_fds_size(), 1u);

    EXPECT_TRUE(poll.rm_write_fd(fd[0]));
    EXPECT_EQ(poll.write_fds_size(), 0u);
    EXPECT_TRUE(poll.rm_read_fd(fd[0]));
    EXPECT_EQ(poll.fds_size(), 0u);
    EXPECT_FALSE(poll.rm_read_fd(fd[0]));
    EXPECT_FALSE(poll.clear_fd(fd[0]));
}

TEST_F(TestEventPoll, test_eventpoll_limit_refuses_only_new_fds)
{
    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;
    ASSERT_TRUE(poll.set_limit(1));

    EXPECT_TRUE(poll.set_read_fd(fd[0]));
    EXPECT_TRUE(poll.set_write_fd(fd[0]));
    EXPECT_FALSE(poll.set_write_fd(fd[1]));

    EXPECT_EQ(poll.fds_size(), 1u);
}

TEST_F(TestEventPoll, test_eventpoll_service_sees_a_fd_added_while_waiting)
{
    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;
    ASSERT_TRUE(poll.set_timeout(10000));
    Synchronizer seen(2);
    std::atomic<bool> signaled = false;
    Handler<EventPoll *> handler([&](EventPoll *polled) {
        for (const PollEvent & event : polled->events())
        {
            if (event.fd == fd[0] && event.readable && !signaled.exchange(true))
                seen.sync();
        }
    });
    poll.add_observer(&handler);
    this->start_service(poll);

    ASSERT_EQ(write_fd(fd[1], "x", 1), 1);
    EXPECT_TRUE(poll.set_read_fd(fd[0]));

    EXPECT_TRUE(seen.sync(Duration(time::sec(2))));
    this->stop_service(poll);
}

TEST_F(TestEventPoll, test_eventpoll_service_stop_skips_the_timeout)
{
    EventPoll poll;
    ASSERT_TRUE(poll.set_timeout(10000));
    this->start_service(poll);

    SteadyClock clock;
    const Timestamp before = clock.now();
    this->stop_service(poll);

    EXPECT_LT(clock.now() - before, Duration(time::sec(1)));
}

TEST_F(TestEventPoll, test_eventpoll_rejects_a_regular_file)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "only epoll refuses regular files";

    File file("/proc/self/exe", "r");
    ASSERT_TRUE(file.is_open());
    EventPoll poll;

    EXPECT_FALSE(poll.set_read_fd(file.fd()));

    EXPECT_EQ(poll.fds_size(), 0u);
}

TEST_F(TestEventPoll, test_eventpoll_reports_hangup_as_closed)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "a closed pipe end is a linux hangup";

    const std::array<int, 2> fd = this->fd_pair();
    EventPoll poll;
    ASSERT_TRUE(poll.set_read_fd(fd[0]));
    close_fd(fd[1]);
    _to_close.pop_back();

    EXPECT_EQ(poll.poll(1000), 1);

    ASSERT_EQ(poll.events().size(), 1u);
    EXPECT_TRUE(poll.events()[0].closed);
}

TEST_F(TestEventPoll, test_eventpoll_watches_a_reused_fd_again)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "fd reuse is a posix behavior";

    std::array<int, 2> first = {-1, -1};
    ASSERT_TRUE(make_fd_pair(first));
    _to_close.push_back(first[1]);
    EventPoll poll;
    ASSERT_TRUE(poll.set_read_fd(first[0]));
    close_fd(first[0]);

    const std::array<int, 2> second = this->fd_pair();
    ASSERT_EQ(second[0], first[0]);
    ASSERT_EQ(write_fd(second[1], "x", 1), 1);

    EXPECT_TRUE(poll.set_read_fd(second[0]));
    EXPECT_EQ(poll.poll(1000), 1);
}

} // namespace test
