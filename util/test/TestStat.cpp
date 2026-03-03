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

} // namespace test
