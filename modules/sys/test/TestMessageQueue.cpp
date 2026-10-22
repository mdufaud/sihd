#include <chrono>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/MessageQueue.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

ArrByteView to_bytes(const char *str)
{
    return ArrByteView((const int8_t *)str, strlen(str));
}

ArrByteView to_bytes(const std::string & str)
{
    return ArrByteView((const int8_t *)str.data(), str.size());
}

} // namespace

class TestMessageQueue: public ::testing::Test
{
    protected:
        TestMessageQueue() { LoggerManager::stream(); }

        virtual ~TestMessageQueue() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!MessageQueue::supported)
            {
                GTEST_SKIP() << "message queue not supported on this platform";
            }
            if constexpr (!build::is_windows)
            {
                if (!sihd::sys::fs::exists("/dev/mqueue"))
                    GTEST_SKIP() << "/dev/mqueue is not mounted";
            }
            // queue names are kernel-wide: a concurrent test runner must not collide
            _prefix = "/sihd_mq_" + std::to_string((int)sihd::sys::os::pid());
        }

        virtual void TearDown()
        {
            if (_prefix.empty())
                return;
            // posix queues outlive the process: unlink even when an assertion cut the test short
            for (const char *suffix :
                 {"", "_bounds", "_full", "_nonblocking", "_attr", "_priority", "_unlink", "_process"})
            {
                (void)MessageQueue::unlink(_prefix + suffix);
            }
        }

        std::string _prefix;
};

TEST_F(TestMessageQueue, test_message_queue)
{
    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix, 4, 64));

    // create is exclusive: posix O_EXCL, windows FILE_FLAG_FIRST_PIPE_INSTANCE (wine does not enforce it)
    MessageQueue second;
    auto exists = second.create(_prefix, 4, 64);
    if (os::is_run_by_wine())
        EXPECT_TRUE(exists);
    else
    {
        ASSERT_FALSE(exists);
        EXPECT_EQ(exists.error().code, ErrorCode::already_exists);
    }

    MessageQueue attached;
    ASSERT_TRUE(attached.open(_prefix));

    // a 0-byte message must stay distinguishable from no message
    EXPECT_FALSE(mq.try_receive().has_value());
    auto none = mq.timed_receive(std::chrono::milliseconds(50));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code, ErrorCode::timeout);

    // fifo across the two handles: posix is one kernel queue, windows one pipe direction each
    ASSERT_TRUE(attached.send(to_bytes("one")));
    ASSERT_TRUE(mq.send(to_bytes("two")));
    auto first = mq.receive();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "one");
    auto msg_two = attached.receive();
    ASSERT_TRUE(msg_two.has_value());
    EXPECT_EQ(*msg_two, "two");

    ASSERT_TRUE(attached.send(to_bytes("")));
    auto empty = mq.receive();
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());
    EXPECT_FALSE(mq.try_receive().has_value());

    EXPECT_TRUE(MessageQueue::unlink(_prefix));
}

TEST_F(TestMessageQueue, test_message_queue_priority)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "named pipes carry no message priority";

    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_priority", 4, 64));

    auto low = mq.send(to_bytes("low"), 1);
    ASSERT_TRUE(low) << low.error().message;
    ASSERT_TRUE(mq.send(to_bytes("high"), 5));
    ASSERT_TRUE(mq.send(to_bytes("mid"), 3));

    unsigned priority = 0;
    auto first = mq.receive(&priority);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "high");
    EXPECT_EQ(priority, 5u);
    auto second = mq.receive(&priority);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, "mid");
    EXPECT_EQ(priority, 3u);
    auto third = mq.receive(&priority);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, "low");
    EXPECT_EQ(priority, 1u);
}

TEST_F(TestMessageQueue, test_message_queue_bounds)
{
    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_bounds", 2, 16));

    MessageQueue peer;
    ASSERT_TRUE(peer.open(_prefix + "_bounds"));

    std::string max_message(16, 'x');
    auto sent = mq.send(to_bytes(max_message));
    ASSERT_TRUE(sent) << sent.error().message;
    if constexpr (!build::is_windows)
    {
        auto oversized = mq.send(to_bytes(std::string(17, 'y')));
        ASSERT_FALSE(oversized);
    }
    auto drained = peer.receive();
    ASSERT_TRUE(drained.has_value());
    EXPECT_EQ(drained->size(), 16u);
}

TEST_F(TestMessageQueue, test_message_queue_try_full)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "windows bounds are buffer flow-control, not a message count";

    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_full", 2, 16));

    ASSERT_TRUE(mq.try_send(to_bytes("a")));
    ASSERT_TRUE(mq.try_send(to_bytes("b")));
    auto full = mq.try_send(to_bytes("c"));
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().code, ErrorCode::would_block);
    auto timed = mq.timed_send(to_bytes("d"), std::chrono::milliseconds(50));
    ASSERT_FALSE(timed);
    EXPECT_EQ(timed.error().code, ErrorCode::timeout);

    ASSERT_TRUE(mq.receive().has_value());
    ASSERT_TRUE(mq.try_send(to_bytes("e")));
}

TEST_F(TestMessageQueue, test_message_queue_nonblocking)
{
    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_nonblocking", 2, 16));

    MessageQueue peer;
    ASSERT_TRUE(peer.open(_prefix + "_nonblocking"));

    ASSERT_TRUE(mq.set_nonblocking(true));
    auto empty = mq.receive();
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code, ErrorCode::would_block);

    ASSERT_TRUE(peer.try_send(to_bytes("x")));
    auto msg = mq.receive();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->size(), 1u);

    ASSERT_TRUE(mq.set_nonblocking(false));
    auto none = mq.timed_receive(std::chrono::milliseconds(20));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code, ErrorCode::timeout);
}

TEST_F(TestMessageQueue, test_message_queue_attributes)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "no message queue attributes on windows";

    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_attr", 4, 64));

    auto attrs = mq.attributes();
    ASSERT_TRUE(attrs.has_value());
    EXPECT_EQ(attrs->max_messages, 4);
    EXPECT_EQ(attrs->max_message_size, 64);
    EXPECT_EQ(attrs->current_messages, 0);

    ASSERT_TRUE(mq.send(to_bytes("a")));
    ASSERT_TRUE(mq.send(to_bytes("b")));
    attrs = mq.attributes();
    ASSERT_TRUE(attrs.has_value());
    EXPECT_EQ(attrs->current_messages, 2);
}

TEST_F(TestMessageQueue, test_message_queue_unlink)
{
    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_unlink", 2, 16));
    ASSERT_TRUE(mq.unlink());

    if constexpr (build::is_windows)
    {
        // unlink is a no-op on windows: the pipe lives until its last handle closes
        ASSERT_TRUE(mq.clear());
    }
    else
    {
        ASSERT_TRUE(mq.send(to_bytes("still-here")));
        auto received = mq.receive();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(*received, "still-here");
        EXPECT_FALSE(MessageQueue::unlink(_prefix + "_unlink"));
    }

    MessageQueue gone;
    EXPECT_FALSE(gone.unlink());
    auto reopened = gone.open(_prefix + "_unlink");
    ASSERT_FALSE(reopened);
    EXPECT_EQ(reopened.error().code, ErrorCode::not_found);
}

TEST_F(TestMessageQueue, test_message_queue_process)
{
    MessageQueue mq;
    ASSERT_TRUE(mq.create(_prefix + "_process", 4, 64));

    std::string child_output;
    proc::Options options;
    options.stderr_callback = [&](std::string_view view) {
        child_output.append(view);
    };
    auto exit_code = proc::execute({helper_path(), "mq-send", _prefix + "_process", "hello"}, options).get();
    auto msg = mq.timed_receive(std::chrono::seconds(5));
    EXPECT_EQ(exit_code & (1 << 1), 1 << 1) << child_output;
    EXPECT_EQ(exit_code & (1 << 2), 1 << 2) << child_output;
    ASSERT_TRUE(msg.has_value()) << (msg ? "" : msg.error().message);
    EXPECT_EQ(*msg, "hello");

    // the receiver is spawned first: its read must be pending when we send
    std::string recv_output;
    proc::Options recv_options;
    recv_options.stderr_callback = [&](std::string_view view) {
        recv_output.append(view);
    };
    auto recv_future = proc::execute({helper_path(), "mq-receive", _prefix + "_process", "world"}, recv_options);
    auto pushed = mq.timed_send(to_bytes("world"), std::chrono::seconds(5));
    ASSERT_TRUE(pushed) << (pushed ? "" : pushed.error().message);
    auto recv_exit = recv_future.get();
    EXPECT_EQ(recv_exit & (1 << 1), 1 << 1) << recv_output;
    EXPECT_EQ(recv_exit & (1 << 2), 1 << 2) << recv_output;

    EXPECT_TRUE(mq.clear());
}

} // namespace test
