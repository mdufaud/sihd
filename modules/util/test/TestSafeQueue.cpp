#include <thread>

#include <fmt/printf.h>
#include <gtest/gtest.h>

#include <sihd/util/SafeQueue.hpp>

namespace test
{
using namespace sihd::util;
class TestSafeQueue: public ::testing::Test
{
    protected:
        TestSafeQueue() = default;

        virtual ~TestSafeQueue() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

struct MoveCounter
{
        int moves = 0;

        MoveCounter() = default;
        MoveCounter(MoveCounter && other): moves(other.moves + 1) {}
        MoveCounter & operator=(MoveCounter && other)
        {
            moves = other.moves + 1;
            return *this;
        }
};

void write_number(SafeQueue<int> & queue, int number, int times)
{
    for (int i = 0; i < times; ++i)
    {
        EXPECT_TRUE(queue.push(number));
    }
}

void pop_all(SafeQueue<int> & queue)
{
    while (!queue.empty())
    {
        queue.try_pop();
    }
}

TEST_F(TestSafeQueue, test_safequeue_terminate)
{
    SafeQueue<int> queue;

    std::thread t1([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fmt::print("Closed pop\n");
        EXPECT_FALSE(queue.pop().has_value());
    });

    fmt::print("Terminating queue\n");
    queue.terminate();

    t1.join();
}

TEST_F(TestSafeQueue, test_safequeue_push_terminated)
{
    SafeQueue<int> queue;

    queue.terminate();

    EXPECT_FALSE(queue.push(1));
    EXPECT_TRUE(queue.empty());
}

TEST_F(TestSafeQueue, test_safequeue_terminate_drains)
{
    SafeQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.terminate();

    EXPECT_EQ(queue.pop_wait().value(), 1);
    EXPECT_EQ(queue.pop_wait().value(), 2);
    EXPECT_EQ(queue.pop_wait().value(), 3);
    EXPECT_FALSE(queue.pop_wait().has_value());
}

TEST_F(TestSafeQueue, test_safequeue_terminate_try_pop_drains)
{
    SafeQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.terminate();

    EXPECT_EQ(queue.try_pop().value(), 1);
    EXPECT_EQ(queue.try_pop().value(), 2);
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST_F(TestSafeQueue, test_safequeue_clear)
{
    SafeQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST_F(TestSafeQueue, test_safequeue_push_no_move_on_refusal)
{
    SafeQueue<MoveCounter> queue;

    MoveCounter accepted;
    EXPECT_TRUE(queue.push(std::move(accepted), 1));

    MoveCounter refused;
    EXPECT_FALSE(queue.push(std::move(refused), 1));
    EXPECT_EQ(refused.moves, 0);

    EXPECT_GE(queue.try_pop()->moves, 1);
}

TEST_F(TestSafeQueue, test_safequeue_space)
{
    SafeQueue<int> queue;

    constexpr size_t maximum_queue_size = 3;

    ASSERT_TRUE(queue.empty());
    EXPECT_TRUE(queue.push(1, maximum_queue_size));
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_TRUE(queue.push(2, maximum_queue_size));
    EXPECT_EQ(queue.size(), 2u);
    EXPECT_TRUE(queue.push(3, maximum_queue_size));
    EXPECT_EQ(queue.size(), 3u);
    EXPECT_EQ(queue.front(), 1);
    EXPECT_EQ(queue.back(), 3);

    EXPECT_FALSE(queue.push(4, maximum_queue_size));
    EXPECT_EQ(queue.size(), 3u);
    EXPECT_EQ(queue.back(), 3);

    fmt::print("Starting thread waiting for space to write to queue\n");

    std::thread t1([&] {
        EXPECT_TRUE(queue.wait_for_space(maximum_queue_size));
        fmt::print("Space found - writing value\n");
        EXPECT_TRUE(queue.push(42));
    });

    fmt::print("Popping queue to make enough room\n");

    EXPECT_EQ(queue.pop().value(), 1);

    t1.join();

    fmt::print("Thread joined\n");

    EXPECT_EQ(queue.size(), 3u);
    EXPECT_EQ(queue.front(), 2);
    EXPECT_EQ(queue.back(), 42);
}

TEST_F(TestSafeQueue, test_safequeue_peek)
{
    SafeQueue<int> queue;

    EXPECT_FALSE(queue.peek_front([](const int &) {}));
    EXPECT_FALSE(queue.peek_back([](const int &) {}));

    queue.push(1);
    queue.push(2);
    queue.push(3);

    EXPECT_TRUE(queue.peek_front([](const int & v) { EXPECT_EQ(v, 1); }));
    EXPECT_TRUE(queue.peek_back([](const int & v) { EXPECT_EQ(v, 3); }));
    EXPECT_EQ(queue.size(), 3u);

    EXPECT_EQ(queue.pop().value(), 1);
    EXPECT_TRUE(queue.peek_front([](const int & v) { EXPECT_EQ(v, 2); }));
}

TEST_F(TestSafeQueue, test_safequeue_pushpop)
{
    SafeQueue<int> queue;

    std::thread t1(write_number, std::ref(queue), 42, 1);

    int number = queue.pop().value();

    EXPECT_EQ(number, 42);

    t1.join();

    EXPECT_FALSE(queue.try_pop().has_value());
    queue.push(10);
    EXPECT_EQ(queue.try_pop().value(), 10);

    constexpr size_t max_queue_size = 10;
    for (int i = 0; i < 100; ++i)
    {
        queue.push(42, max_queue_size);
    }

    EXPECT_EQ(queue.size(), 10u);
}

TEST_F(TestSafeQueue, test_safequeue_spam)
{
    SafeQueue<int> queue;

    std::thread t1(write_number, std::ref(queue), 42, 100);
    std::thread t2(write_number, std::ref(queue), 1337, 100);
    std::thread t3(write_number, std::ref(queue), 420, 100);
    t3.join();
    t2.join();
    t1.join();

    EXPECT_EQ(queue.size(), 300u);

    size_t ft_count = 0;
    size_t leet_count = 0;
    size_t blazeit_count = 0;

    while (!queue.empty())
    {
        int number = queue.pop().value();
        if (number == 42)
            ft_count++;
        else if (number == 1337)
            leet_count++;
        else if (number == 420)
            blazeit_count++;
    }

    EXPECT_EQ(queue.size(), 0u);
    EXPECT_EQ(ft_count, 100u);
    EXPECT_EQ(leet_count, 100u);
    EXPECT_EQ(blazeit_count, 100u);

    std::thread t4(write_number, std::ref(queue), 42, 200);
    std::thread t5(write_number, std::ref(queue), 1337, 200);
    std::thread t6(write_number, std::ref(queue), 420, 200);
    t6.join();
    t5.join();
    t4.join();

    EXPECT_EQ(queue.size(), 600u);

    std::thread t7(pop_all, std::ref(queue));
    std::thread t8(pop_all, std::ref(queue));
    std::thread t9(pop_all, std::ref(queue));
    t9.join();
    t8.join();
    t7.join();

    EXPECT_EQ(queue.size(), 0u);
}

} // namespace test
