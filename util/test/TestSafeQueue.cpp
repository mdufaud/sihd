#include <thread>

#include <fmt/printf.h>
#include <gtest/gtest.h>

#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/Synchronizer.hpp>

namespace test
{
using namespace sihd::util;
class TestSafeQueue: public ::testing::Test
{
    protected:
        TestSafeQueue() {}

        virtual ~TestSafeQueue() {}

        virtual void SetUp() {}

        virtual void TearDown() {}
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
        fmt::print("Infinite pop\n");
        EXPECT_THROW(queue.pop(), std::invalid_argument);
    });

    fmt::print("Terminating queue\n");
    queue.terminate();

    t1.join();
}

TEST_F(TestSafeQueue, test_safequeue_space)
{
    SafeQueue<int> queue;

    constexpr size_t maximum_queue_size = 3;

    ASSERT_TRUE(queue.empty());
    EXPECT_TRUE(queue.push(1, maximum_queue_size));
    EXPECT_EQ(queue.size(), 1);
    EXPECT_TRUE(queue.push(2, maximum_queue_size));
    EXPECT_EQ(queue.size(), 2);
    EXPECT_TRUE(queue.push(3, maximum_queue_size));
    EXPECT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.front(), 1);
    EXPECT_EQ(queue.back(), 3);

    EXPECT_FALSE(queue.push(4, maximum_queue_size));
    EXPECT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.back(), 3);

    fmt::print("Starting thread waiting for space to write to queue\n");

    std::thread t1([&] {
        EXPECT_TRUE(queue.wait_for_space(maximum_queue_size));
        fmt::print("Space found - writing value\n");
        EXPECT_TRUE(queue.push(42));
    });

    fmt::print("Popping queue to make enough room\n");

    queue.pop();

    t1.join();

    fmt::print("Thread joined\n");

    EXPECT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.front(), 2);
    EXPECT_EQ(queue.back(), 42);
}

TEST_F(TestSafeQueue, test_safequeue_pushpop)
{
    SafeQueue<int> queue;

    std::thread t1(write_number, std::ref(queue), 42, 1);

    int number = queue.pop();

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

    EXPECT_EQ(queue.size(), 10);
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

    EXPECT_EQ(queue.size(), 300);

    size_t ft_count = 0;
    size_t leet_count = 0;
    size_t blazeit_count = 0;

    while (!queue.empty())
    {
        int number = queue.pop();
        if (number == 42)
            ft_count++;
        else if (number == 1337)
            leet_count++;
        else if (number == 420)
            blazeit_count++;
    }

    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(ft_count, 100);
    EXPECT_EQ(leet_count, 100);
    EXPECT_EQ(blazeit_count, 100);

    std::thread t4(write_number, std::ref(queue), 42, 200);
    std::thread t5(write_number, std::ref(queue), 1337, 200);
    std::thread t6(write_number, std::ref(queue), 420, 200);
    t6.join();
    t5.join();
    t4.join();

    EXPECT_EQ(queue.size(), 600);

    std::thread t7(pop_all, std::ref(queue));
    std::thread t8(pop_all, std::ref(queue));
    std::thread t9(pop_all, std::ref(queue));
    t9.join();
    t8.join();
    t7.join();

    EXPECT_EQ(queue.size(), 0);
}

} // namespace test
