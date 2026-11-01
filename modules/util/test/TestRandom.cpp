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

TEST_F(TestRandom, test_random_seeds_differ)
{
    Random first(1);
    Random second(2);

    int same = 0;
    for (int i = 0; i < 100; ++i)
        same += first.rand() == second.rand() ? 1 : 0;
    EXPECT_EQ(same, 0);
}

TEST_F(TestRandom, test_seed_changes_every_call)
{
    EXPECT_NE(num::seed(), num::seed());
}

} // namespace test
