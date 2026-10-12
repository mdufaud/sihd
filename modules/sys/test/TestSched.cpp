#include <gtest/gtest.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/sched.hpp>
#include <sihd/util/build.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::sys;

class TestSched: public ::testing::Test
{
    protected:
        TestSched() = default;
        virtual ~TestSched() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestSched, test_sched_nice)
{
    auto current = sched::nice(os::pid());
    ASSERT_TRUE(current.has_value());

    // raising the niceness (lower priority) needs no privilege
    ASSERT_TRUE(sched::set_nice(os::pid(), *current + 1).has_value());
    auto raised = sched::nice(os::pid());
    ASSERT_TRUE(raised.has_value());
    if constexpr (sihd::util::build::is_windows)
        // priority classes are coarse: the value may round to the same anchor
        EXPECT_GE(*raised, *current);
    else
        EXPECT_EQ(*raised, *current + 1);

    // going back may fail without the privilege to raise the priority
    (void)sched::set_nice(os::pid(), *current);
}

TEST_F(TestSched, test_sched_scheduler)
{
    if constexpr (sihd::util::build::is_windows || sihd::util::build::is_emscripten)
        GTEST_SKIP() << "scheduler policies are a linux feature";

    auto probe = sched::scheduler(os::pid());
    if (probe.has_value() == false)
        GTEST_SKIP() << "sched syscalls unavailable: " << probe.error().message;
    auto priority = sched::scheduler_priority(os::pid());
    ASSERT_TRUE(priority.has_value()) << priority.error().message;

    auto applied = sched::set_scheduler(os::pid(), sched::Policy::batch, sched::Priority {});
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    auto changed = sched::scheduler(os::pid());
    ASSERT_TRUE(changed.has_value()) << changed.error().message;
    EXPECT_EQ(*changed, sched::Policy::batch);

    auto restored = sched::set_scheduler(os::pid(), *probe, *priority);
    ASSERT_TRUE(restored.has_value()) << restored.error().message;

    auto deadline = sched::set_scheduler(os::pid(), sched::Policy::deadline, sched::Priority {});
    ASSERT_FALSE(deadline.has_value());
    EXPECT_EQ(deadline.error().code, invalid_argument);
}

TEST_F(TestSched, test_sched_affinity)
{
    auto mask = sched::affinity(os::pid());
    ASSERT_TRUE(mask.has_value()) << mask.error().message;
    EXPECT_FALSE(mask->empty());

    ASSERT_TRUE(sched::set_affinity(os::pid(), *mask).has_value());
    auto after = sched::affinity(os::pid());
    ASSERT_TRUE(after.has_value()) << after.error().message;
    EXPECT_EQ(*after, *mask);

    auto empty = sched::set_affinity(os::pid(), {});
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, invalid_argument);
}

} // namespace test
