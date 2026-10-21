#include <csignal>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/linux/prctl.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

class TestPrctl: public ::testing::Test
{
    protected:
        TestPrctl() { LoggerManager::stream(); }

        virtual ~TestPrctl() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!prctl::supported)
            {
                GTEST_SKIP() << "prctl not supported on this platform";
            }
        }

        virtual void TearDown()
        {
            (void)prctl::set_pdeathsig(0);
            (void)prctl::set_child_subreaper(false);
        }
};

TEST_F(TestPrctl, test_pdeathsig)
{
    ASSERT_TRUE(prctl::set_pdeathsig(SIGUSR1));
    auto set = prctl::pdeathsig();
    ASSERT_TRUE(set.has_value());
    EXPECT_EQ(*set, SIGUSR1);

    ASSERT_TRUE(prctl::set_pdeathsig(0));
    auto cleared = prctl::pdeathsig();
    ASSERT_TRUE(cleared.has_value());
    EXPECT_EQ(*cleared, 0);

    auto invalid = prctl::set_pdeathsig(-1);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestPrctl, test_dumpable)
{
    auto original = prctl::dumpable();
    ASSERT_TRUE(original.has_value());

    ASSERT_TRUE(prctl::set_dumpable(!*original));
    auto toggled = prctl::dumpable();
    ASSERT_TRUE(prctl::set_dumpable(*original));

    ASSERT_TRUE(toggled.has_value());
    EXPECT_EQ(*toggled, !*original);
    auto restored = prctl::dumpable();
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, *original);
}

TEST_F(TestPrctl, test_child_subreaper)
{
    ASSERT_TRUE(prctl::set_child_subreaper(true));
    auto set = prctl::child_subreaper();
    ASSERT_TRUE(set.has_value());
    EXPECT_TRUE(*set);

    ASSERT_TRUE(prctl::set_child_subreaper(false));
    auto cleared = prctl::child_subreaper();
    ASSERT_TRUE(cleared.has_value());
    EXPECT_FALSE(*cleared);
}

TEST_F(TestPrctl, test_no_new_privs)
{
    auto parent_before = prctl::no_new_privs();
    ASSERT_TRUE(parent_before.has_value());

    Process proc([]() -> int {
        if (!prctl::set_no_new_privs())
            return 1;
        auto locked = prctl::no_new_privs();
        return locked.has_value() && *locked ? 0 : 2;
    });
    proc.set_force_fork(true);
    ASSERT_TRUE(proc.execute());
    ASSERT_TRUE(proc.wait_exit().value_or(false));

    EXPECT_EQ(static_cast<int>(proc.return_code()), 0);
    auto parent_after = prctl::no_new_privs();
    ASSERT_TRUE(parent_after.has_value());
    EXPECT_EQ(*parent_after, *parent_before);
}

} // namespace test
