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

    std::atomic<int> count = 0;
    for (size_t i = 0; i < total_jobs; ++i)
    {
        pool.add_job([&count] { ++count; });
    }

    pool.wait_all_jobs();
    pool.stop();

    SIHD_LOG_INFO("Total: {}", count.load());
    EXPECT_EQ(count.load(), total_jobs);

    auto stats = pool.stats();
    for (size_t i = 0; i < stats.size(); i++)
    {
        SIHD_LOG_INFO("Thread[{}]: {}", i + 1, stats[i].samples);
    }

    const auto samples = container::sum(stats, [](const auto & stat) { return stat.samples; });
    EXPECT_EQ(samples, total_jobs);
}

TEST_F(TestThreadPool, test_threadpool_future)
{
    constexpr size_t total_jobs = 1000;

    std::vector<size_t> to_fill;
    to_fill.resize(total_jobs);

    ThreadPool pool("thread_pool", 4);

    std::vector<size_t> results = pool.complete_all_jobs(1000, [&to_fill](size_t thread_n) {
        to_fill[thread_n] = thread_n * 2;
        return thread_n;
    });

    ASSERT_EQ(to_fill.size(), results.size());

    for (size_t i = 0; i < results.size(); ++i)
    {
        ASSERT_EQ(to_fill[i], results[i] * 2);
    }
}

} // namespace test
