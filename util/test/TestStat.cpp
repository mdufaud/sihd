#include <gtest/gtest.h>

#include <sihd/util/Stat.hpp>

namespace test
{
using namespace sihd::util;

class TestStat: public ::testing::Test
{
    protected:
        TestStat() = default;
        virtual ~TestStat() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestStat, test_stat_basic)
{
    Stat<double> stat;

    EXPECT_EQ(stat.samples, 0u);
    EXPECT_DOUBLE_EQ(stat.average(), 0.0);
    EXPECT_DOUBLE_EQ(stat.variance(), 0.0);

    stat.add_sample(10.0);
    stat.add_sample(20.0);
    stat.add_sample(30.0);

    EXPECT_EQ(stat.samples, 3u);
    EXPECT_DOUBLE_EQ(stat.min, 10.0);
    EXPECT_DOUBLE_EQ(stat.max, 30.0);
    EXPECT_DOUBLE_EQ(stat.sum, 60.0);
    EXPECT_DOUBLE_EQ(stat.average(), 20.0);
}

TEST_F(TestStat, test_stat_variance)
{
    Stat<double> stat;

    stat.add_sample(2.0);
    stat.add_sample(4.0);
    stat.add_sample(4.0);
    stat.add_sample(4.0);
    stat.add_sample(5.0);
    stat.add_sample(5.0);
    stat.add_sample(7.0);
    stat.add_sample(9.0);

    EXPECT_DOUBLE_EQ(stat.average(), 5.0);
    EXPECT_DOUBLE_EQ(stat.variance(), 4.0);
    EXPECT_DOUBLE_EQ(stat.standard_deviation(), 2.0);
}

TEST_F(TestStat, test_stat_clear)
{
    Stat<int> stat;
    stat.add_sample(5);
    stat.add_sample(10);
    stat.clear();

    EXPECT_EQ(stat.samples, 0u);
    EXPECT_EQ(stat.min, 0);
    EXPECT_EQ(stat.max, 0);
    EXPECT_EQ(stat.sum, 0);
}

TEST_F(TestStat, test_stat_single_sample)
{
    Stat<double> stat;
    stat.add_sample(42.0);

    EXPECT_EQ(stat.samples, 1u);
    EXPECT_DOUBLE_EQ(stat.min, 42.0);
    EXPECT_DOUBLE_EQ(stat.max, 42.0);
    EXPECT_DOUBLE_EQ(stat.average(), 42.0);
    EXPECT_DOUBLE_EQ(stat.variance(), 0.0);
}

// Stat uses inverse-normal (z-score) approximation for percentiles.
// Symmetric uniform distribution [1..200]: mean=100.5, stddev~57.7
// p95 = mean + 1.645*stddev ~ 195, p99 = mean + 2.326*stddev ~ 235 (capped by distribution)
TEST_F(TestStat, test_gaussian_stat_percentiles)
{
    Stat<double> stat;
    for (int i = 1; i <= 200; ++i)
        stat.add_sample(static_cast<double>(i));

    EXPECT_NEAR(stat.median(), 100.5, 1.0);
    EXPECT_GT(stat.p95(), 150.0);
    EXPECT_GT(stat.p99(), 180.0);
}

// PSquareStat: accurate online estimator for asymmetric distributions.
// Uniform [1..100]: exact median=50, p95~95, p99~99; P-Square has bounded approximation error.
TEST_F(TestStat, test_psquare_stat_percentiles)
{
    PSquareStat<long long> stat;
    for (long long i = 1; i <= 100; ++i)
        stat.add_sample(i);

    EXPECT_NEAR(stat.median(), 50LL, 5LL);
    EXPECT_GE(stat.p95(), 90LL);
    EXPECT_GE(stat.p99(), 95LL);
}

// PSquareStat clear resets estimators: fresh run after clear gives same results.
TEST_F(TestStat, test_psquare_stat_clear)
{
    PSquareStat<long long> stat;
    for (long long i = 1; i <= 100; ++i)
        stat.add_sample(i);

    stat.clear();
    EXPECT_EQ(stat.samples, 0u);

    for (long long i = 1; i <= 100; ++i)
        stat.add_sample(i);

    EXPECT_NEAR(stat.median(), 50LL, 5LL);
}

} // namespace test