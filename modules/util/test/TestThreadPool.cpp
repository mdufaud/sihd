#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/ThreadPool.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/container.hpp>

#include "wait/wait_for.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestThreadPool: public ::testing::Test
{
    protected:
        TestThreadPool() { LoggerManager::stream(); }

        virtual ~TestThreadPool() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestThreadPool, test_threadpool_spam)
{
    constexpr size_t total_jobs = 1000;

    ThreadPool pool("thread_pool", 4);

    auto future = pool.add_job([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return 42;
    });

    std::atomic<int> count = 0;
    for (size_t i = 0; i < total_jobs; ++i)
    {
        pool.add_job([&count] { ++count; });
    }

    future.wait();
    EXPECT_EQ(future.get(), 42);

    pool.wait_all_jobs();
    pool.stop();

    SIHD_LOG(info, "Total: {}", count.load());
    EXPECT_EQ(count.load(), (int)total_jobs);

    auto stats = pool.stats();
    for (size_t i = 0; i < stats.size(); i++)
    {
        SIHD_LOG(info, "Thread[{}]: {}", i + 1, stats[i].samples);
    }

    const auto samples = container::sum(stats, [](const auto & stat) { return stat.samples; });
    EXPECT_EQ(samples, total_jobs + 1);
}

TEST_F(TestThreadPool, test_threadpool_future)
{
    constexpr size_t total_jobs = 500;

    std::vector<size_t> to_fill;
    to_fill.resize(total_jobs);

    ThreadPool pool("thread_pool", 4);

    pool.complete_all_jobs(total_jobs, [&to_fill](size_t thread_n) { to_fill[thread_n] = thread_n; });

    std::vector<size_t> results = pool.complete_all_jobs(total_jobs, [](size_t thread_n) { return thread_n * 2; });

    ASSERT_EQ(to_fill.size(), results.size());

    for (size_t i = 0; i < results.size(); ++i)
    {
        ASSERT_EQ(to_fill[i], results[i] / 2);
    }
}

TEST_F(TestThreadPool, test_threadpool_error)
{
    std::atomic<int> executed = 0;
    ThreadPool pool("thread_pool", 1);

    pool.add_job([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });

    auto discarded = pool.add_job([&]() { ++executed; });

    pool.stop();

    ASSERT_THROW(discarded.get(), std::future_error);
    EXPECT_EQ(executed.load(), 0);
}

TEST_F(TestThreadPool, test_threadpool_remaining_jobs)
{
    ThreadPool pool("thread_pool_remaining", 1);
    std::atomic<int> count = 0;

    for (size_t i = 0; i < 10; ++i)
    {
        pool.add_job([&count] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++count;
        });
    }

    EXPECT_GT(pool.remaining_jobs(), 0u);
    pool.wait_all_jobs();
    EXPECT_EQ(pool.remaining_jobs(), 0u);
    EXPECT_TRUE(wait_for([&] { return count.load() == 10; }));

    pool.stop();
}

TEST_F(TestThreadPool, test_threadpool_multi_producer)
{
    ThreadPool pool("thread_pool_producers", 4);
    std::atomic<int> count = 0;

    {
        std::vector<std::thread> producers;
        for (size_t producer = 0; producer < 4; ++producer)
        {
            producers.emplace_back([&pool, &count] {
                for (size_t i = 0; i < 250; ++i)
                    pool.add_job([&count] { ++count; });
            });
        }
        for (std::thread & producer : producers)
            producer.join();
    }

    pool.wait_all_jobs();
    EXPECT_TRUE(wait_for([&] { return count.load() == 1000; }));

    pool.stop();
}

TEST_F(TestThreadPool, test_threadpool_stop_completes_inflight)
{
    ThreadPool pool("thread_pool_inflight", 1);
    std::atomic<bool> started = false;
    std::atomic<bool> done = false;

    auto future = pool.add_job([&] {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        done = true;
    });

    EXPECT_TRUE(wait_for([&] { return started.load(); }));
    pool.stop();

    EXPECT_TRUE(done.load());
    EXPECT_NO_THROW(future.get());
}

TEST_F(TestThreadPool, test_threadpool_destructor_stops)
{
    std::atomic<bool> started = false;
    std::atomic<bool> done = false;

    {
        ThreadPool pool("thread_pool_dtor", 1);
        pool.add_job([&] {
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            done = true;
        });
        EXPECT_TRUE(wait_for([&] { return started.load(); }));
    }

    EXPECT_TRUE(done.load());
}

} // namespace test
