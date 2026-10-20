#include <gtest/gtest.h>

#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sihd/sys/linux/cgroup.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

class TestCGroup: public ::testing::Test
{
    protected:
        TestCGroup() { LoggerManager::stream(); }

        virtual ~TestCGroup() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!cgroup::supported)
            {
                GTEST_SKIP() << "cgroup not supported on this platform";
            }
            if (!cgroup::is_v2_available())
                GTEST_SKIP() << "no cgroup v2 hierarchy";
            auto group = cgroup::current_group();
            ASSERT_TRUE(group.has_value());
            if (*group == "/")
                GTEST_SKIP() << "process lives in the root cgroup, which has no limit files";
        }

        virtual void TearDown() {}
};

TEST_F(TestCGroup, test_current_group)
{
    auto group = cgroup::current_group();
    ASSERT_TRUE(group.has_value());
    EXPECT_TRUE(group->starts_with("/"));

    auto path = cgroup::current_group_path();
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(*path, "/sys/fs/cgroup" + *group);
    EXPECT_TRUE(fs::is_dir(*path));
}

TEST_F(TestCGroup, test_memory)
{
    auto current = cgroup::memory_current();
    if (!current && current.error().code == ErrorCode::not_found)
        GTEST_SKIP() << "memory controller not enabled for this group";
    ASSERT_TRUE(current.has_value()) << current.error().message;
    EXPECT_GT(*current, 0u);

    auto maximum = cgroup::memory_max();
    ASSERT_TRUE(maximum.has_value()) << maximum.error().message;
    if (maximum->has_value())
    {
        EXPECT_GT(**maximum, 0u);
    }

    auto rewritten = cgroup::write_memory_max(*maximum);
    if (!rewritten && rewritten.error().code == ErrorCode::permission_denied)
        GTEST_SKIP() << "cgroup not delegated";
    ASSERT_TRUE(rewritten.has_value()) << rewritten.error().message;
    auto reread = cgroup::memory_max();
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(*reread, *maximum);
}

TEST_F(TestCGroup, test_cpu)
{
    auto limit = cgroup::cpu_max();
    if (!limit && limit.error().code == ErrorCode::not_found)
        GTEST_SKIP() << "cpu controller not enabled for this group";
    ASSERT_TRUE(limit.has_value()) << limit.error().message;
    EXPECT_GT(limit->period_us, 0u);

    auto rewritten = cgroup::write_cpu_max(limit->quota_us, limit->period_us);
    if (!rewritten && rewritten.error().code == ErrorCode::permission_denied)
        GTEST_SKIP() << "cgroup not delegated";
    ASSERT_TRUE(rewritten.has_value()) << rewritten.error().message;
    auto reread = cgroup::cpu_max();
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->quota_us, limit->quota_us);
    EXPECT_EQ(reread->period_us, limit->period_us);
}

} // namespace test

#endif
