#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Synchronizer.hpp>

namespace test
{
using namespace sihd::util;

class TestSynchronizer: public ::testing::Test
{
    protected:
        TestSynchronizer() = default;
        virtual ~TestSynchronizer() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestSynchronizer, test_synchronizer_basic)
{
    constexpr int nthreads = 4;
    Synchronizer sync;

    EXPECT_TRUE(sync.init_sync(nthreads));
    EXPECT_FALSE(sync.init_sync(nthreads));
    EXPECT_EQ(sync.total_sync(), nthreads);

    std::atomic<int> counter {0};
    std::vector<std::thread> threads;

    for (int i = 0; i < nthreads; ++i)
    {
        threads.emplace_back([&sync, &counter] {
            counter.fetch_add(1);
            sync.sync();
        });
    }

    for (auto & t : threads)
        t.join();

    EXPECT_EQ(counter.load(), nthreads);
    EXPECT_EQ(sync.current_sync(), 0);
}

TEST_F(TestSynchronizer, test_synchronizer_reset)
{
    Synchronizer sync;
    EXPECT_TRUE(sync.init_sync(2));
    sync.reset();
    EXPECT_EQ(sync.total_sync(), 0);
    EXPECT_TRUE(sync.init_sync(3));
    EXPECT_EQ(sync.total_sync(), 3);
}

TEST_F(TestSynchronizer, test_synchronizer_multi_round)
{
    constexpr int nthreads = 3;
    Synchronizer sync;
    EXPECT_TRUE(sync.init_sync(nthreads));

    auto run_round = [&] {
        std::atomic<int> counter {0};
        std::vector<std::thread> threads;
        for (int i = 0; i < nthreads; ++i)
        {
            threads.emplace_back([&sync, &counter] {
                counter.fetch_add(1);
                sync.sync();
            });
        }
        for (auto & t : threads)
            t.join();
        return counter.load();
    };

    EXPECT_EQ(run_round(), nthreads);
    EXPECT_EQ(sync.current_sync(), 0);

    // second round after re-init
    sync.reset();
    EXPECT_TRUE(sync.init_sync(nthreads));
    EXPECT_EQ(run_round(), nthreads);
    EXPECT_EQ(sync.current_sync(), 0);
}

TEST_F(TestSynchronizer, test_synchronizer_timeout)
{
    Synchronizer sync;
    EXPECT_TRUE(sync.init_sync(2));

    // only one participant arrives - must time out
    EXPECT_FALSE(sync.sync(std::chrono::milliseconds(20)));
}

TEST_F(TestSynchronizer, test_synchronizer_timeout_success)
{
    constexpr int nthreads = 3;
    Synchronizer sync;
    EXPECT_TRUE(sync.init_sync(nthreads));

    std::atomic<int> synced {0};
    std::vector<std::thread> threads;
    for (int i = 0; i < nthreads; ++i)
    {
        threads.emplace_back([&] {
            if (sync.sync(std::chrono::seconds(5)))
                synced.fetch_add(1);
        });
    }
    for (auto & t : threads)
        t.join();

    EXPECT_EQ(synced.load(), nthreads);
    EXPECT_EQ(sync.current_sync(), 0);
}

} // namespace test
