#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/util/ThreadPool.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestThreadPool: public ::testing::Test
{
    protected:
        TestThreadPool() { sihd::util::LoggerManager::basic(); }

        virtual ~TestThreadPool() { sihd::util::LoggerManager::clear_loggers(); }

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

    SIHD_LOG_INFO("Total: {}", count.load());
    EXPECT_EQ(count.load(), (int)total_jobs);

    auto stats = pool.stats();
    for (size_t i = 0; i < stats.size(); i++)
    {
        SIHD_LOG_INFO("Thread[{}]: {}", i + 1, stats[i].samples);
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
    ThreadPool pool("thread_pool", 1);

    pool.add_job([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });

    auto future = pool.add_job([]() { return 42; });

    pool.stop();

    ASSERT_THROW(future.get(), std::future_error);
}

} // namespace test
