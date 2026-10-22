#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/profiling.hpp>

namespace test
{
using namespace sihd::util;

class TestProfiling: public ::testing::Test
{
    protected:
        TestProfiling() { LoggerManager::stream(); }
        virtual ~TestProfiling() { LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestProfiling, test_timeit)
{
    {
        Timeit timeit("test_function");
    }
    Timeit timeit;
    EXPECT_GE(timeit.elapsed(), Timestamp(0));
}

TEST_F(TestProfiling, test_perf_guard)
{
    Perf perf("test_perf");

    for (int i = 0; i < 10; ++i)
    {
        Perf::Guard guard(perf);
        volatile int x = 0;
        for (int j = 0; j < 100; ++j)
            x = x + j;
        (void)x;
    }

    EXPECT_EQ(perf.stat().samples, 10u);
    EXPECT_GE(perf.stat().min, 0);
    EXPECT_GE(perf.stat().max, perf.stat().min);

    perf.log();
}

TEST_F(TestProfiling, test_perf_end_before_begin)
{
    Perf perf("fail");
    EXPECT_THROW(perf.end(), std::runtime_error);
}

TEST_F(TestProfiling, test_detailed_stat_percentiles)
{
    PSquareStat<long long> stat;
    for (long long i = 1; i <= 100; ++i)
        stat.add_sample(i);

    EXPECT_EQ(stat.samples, 100u);
    EXPECT_EQ(stat.min, 1LL);
    EXPECT_EQ(stat.max, 100LL);
    EXPECT_NEAR(stat.median(), 50LL, 5LL);
    EXPECT_GE(stat.p99(), 95LL);
}

} // namespace test
