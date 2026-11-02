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

    EXPECT_EQ(first.rand(), second.rand());
    EXPECT_EQ(first.frand(), second.frand());
    EXPECT_EQ(first.drand(-10.0, 10.0), second.drand(-10.0, 10.0));
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

    // past 32 bits: long is 32 bits on arm32 and on windows
    const uint64_t big = uint64_t {1} << 40;
    const uint64_t value = random.rand(big + 1, big);
    EXPECT_GE(value, big);
    EXPECT_LE(value, big + 1);
    EXPECT_EQ(random.rand(7, 7), 7u);

    const float fvalue = random.frand(1.0f, -1.0f);
    EXPECT_GE(fvalue, -1.0f);
    EXPECT_LE(fvalue, 1.0f);
}

TEST_F(TestRandom, test_seed_changes_every_call)
{
    EXPECT_NE(num::seed(), num::seed());
}

} // namespace test
