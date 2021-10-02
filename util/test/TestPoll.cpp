#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Task.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestPoll:   public ::testing::Test
    {
        protected:
            TestPoll()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestPoll()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                _timeout = 0;
                _read_count = 0;
                _write_count = 0;
            }

            virtual void TearDown()
            {
            }

            int _timeout;
            int _read_count;
            int _write_count;
    };

    TEST_F(TestPoll, test_poll)
    {
        Poll poll;
        poll.set_max_fds(5);

        int fd[2];
        EXPECT_TRUE(pipe(fd) >= 0);
        poll.set_read_fd(fd[0]);
        poll.set_write_fd(fd[1]);

        auto write_handler = new Handler<int>([this] (int fd)
        {
            TRACE("Writing in fd: " << fd);
            int ret = write(fd, "hello world", 11);
            TRACE("Wrote " << ret << " bytes: 'hello world'");
            _write_count += 1;
        });
        char buffer[20];
        bzero(buffer, 20);
        auto read_handler = new Handler<int>([&buffer, this] (int fd)
        {
            TRACE("Reading in fd: " << fd);
            int ret = read(fd, buffer, 20);
            buffer[ret] = 0;
            TRACE("Read " << ret << " bytes: '" << buffer << "'");
            _read_count += 1;
        });
        auto timeout_handler = new Handler<int>([this] (int timeout)
        {
            TRACE("Timedout: " << timeout << " ms");
            _timeout = timeout;
        });
        poll.set_handlers(read_handler, write_handler, timeout_handler);

        // first write from writing end of pipe
        int res = poll.poll(10);
        // expect the write fd
        EXPECT_EQ(res, 1);
        // clearing the write fd so it does not write again
        poll.clear_fd(fd[1]);

        // nothing read yet
        EXPECT_EQ(strlen(buffer), 0u);
        EXPECT_EQ(_write_count, 1);
        EXPECT_EQ(_read_count, 0);
        EXPECT_EQ(_timeout, 0);

        // read from reading end of pipe
        res = poll.poll(10);
        // expect the read fd
        EXPECT_EQ(res, 1);

        // buffer filed
        EXPECT_EQ(strcmp(buffer, "hello world"), 0);
        EXPECT_EQ(_write_count, 1);
        EXPECT_EQ(_read_count, 1);
        EXPECT_EQ(_timeout, 0);

        // expect timeout
        res = poll.poll(10);
        EXPECT_EQ(res, 0);

        EXPECT_EQ(_write_count, 1);
        EXPECT_EQ(_read_count, 1);
        EXPECT_EQ(_timeout, 10);
    }
}
