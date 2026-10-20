#include <gtest/gtest.h>

#include <sihd/util/Random.hpp>
#include <sihd/util/num.hpp>

namespace test
{
using namespace sihd::util;
class TestRandom: public ::testing::Test
{
    protected:
        TestRandom() = default;
        virtual ~TestRandom() = default;
};

TEST_F(TestRandom, test_random_same_seed_same_sequence)
{
    Random first(42);
    Random second(42);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(first.rand(), second.rand());
        EXPECT_EQ(first.frand(), second.frand());
        EXPECT_EQ(first.drand(-10.0, 10.0), second.drand(-10.0, 10.0));
    }
}

TEST_F(TestRandom, test_random_seeds_differ)
{
    Random first(1);
    Random second(2);

    int same = 0;
    for (int i = 0; i < 100; ++i)
        same += first.rand() == second.rand() ? 1 : 0;
    EXPECT_EQ(same, 0);
}

TEST_F(TestRandom, test_random_bounds)
{
    Random random(num::seed());

    for (int i = 0; i < 1000; ++i)
    {
        const uint64_t value = random.rand(100, 10);
        EXPECT_GE(value, 10u);
        EXPECT_LE(value, 100u);

        const float fvalue = random.frand(1.0f, -1.0f);
        EXPECT_GE(fvalue, -1.0f);
        EXPECT_LE(fvalue, 1.0f);
    }
    EXPECT_EQ(random.rand(7, 7), 7u);
}

TEST_F(TestRandom, test_seed_changes_every_call)
{
    EXPECT_NE(num::seed(), num::seed());
}

} // namespace test
