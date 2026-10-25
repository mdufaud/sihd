#include <array>

#include <gtest/gtest.h>

#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include "userns_child.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

namespace
{

constexpr std::array<ns::Type, 10> all_types = {
    ns::Type::cgroup,
    ns::Type::ipc,
    ns::Type::mount,
    ns::Type::net,
    ns::Type::pid,
    ns::Type::pid_for_children,
    ns::Type::time,
    ns::Type::time_for_children,
    ns::Type::user,
    ns::Type::uts,
};

constexpr int child_ok = 0;
constexpr int child_id_failed = 1;
constexpr int child_ns_unchanged = 2;
constexpr int child_wrong_joined_ns = 3;
constexpr int child_error_base = 10;

int child_code(ErrorCode code)
{
    return child_error_base + static_cast<int>(code);
}

} // namespace

class TestNs: public ::testing::Test
{
    protected:
        TestNs() { LoggerManager::stream(); }

        virtual ~TestNs() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!ns::supported)
            {
                GTEST_SKIP() << "namespaces not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestNs, test_to_str)
{
    EXPECT_EQ(ns::to_str(ns::Type::cgroup), "cgroup");
    EXPECT_EQ(ns::to_str(ns::Type::ipc), "ipc");
    EXPECT_EQ(ns::to_str(ns::Type::mount), "mnt");
    EXPECT_EQ(ns::to_str(ns::Type::net), "net");
    EXPECT_EQ(ns::to_str(ns::Type::pid), "pid");
    EXPECT_EQ(ns::to_str(ns::Type::pid_for_children), "pid_for_children");
    EXPECT_EQ(ns::to_str(ns::Type::time), "time");
    EXPECT_EQ(ns::to_str(ns::Type::time_for_children), "time_for_children");
    EXPECT_EQ(ns::to_str(ns::Type::user), "user");
    EXPECT_EQ(ns::to_str(ns::Type::uts), "uts");
}

TEST_F(TestNs, test_id)
{
    for (const ns::Type type : all_types)
    {
        auto self = ns::id(type);
        const bool optional_type = type == ns::Type::time || type == ns::Type::time_for_children;
        if (optional_type && !self && self.error().code == ErrorCode::not_found)
            continue;
        ASSERT_TRUE(self.has_value()) << ns::to_str(type) << ": " << self.error().message;
        EXPECT_GT(*self, 0u);

        auto by_pid = ns::id(type, os::pid());
        ASSERT_TRUE(by_pid.has_value());
        EXPECT_EQ(*by_pid, *self);
    }

    auto missing = ns::id(ns::Type::net, -1);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::not_found);
}

TEST_F(TestNs, test_unshare_and_setns)
{
    auto user_before = ns::id(ns::Type::user);
    auto uts_before = ns::id(ns::Type::uts);
    ASSERT_TRUE(user_before.has_value());
    ASSERT_TRUE(uts_before.has_value());
    constexpr std::array<ns::Type, 1> types = {ns::Type::uts};

    expect_in_userns(types, [&user_before, &uts_before]() -> int {
        auto user_after = ns::id(ns::Type::user);
        auto uts_after = ns::id(ns::Type::uts);
        if (!user_after || !uts_after)
            return child_id_failed;
        if (*user_before == *user_after || *uts_before == *uts_after)
            return child_ns_unchanged;

        auto joined = ns::setns("/proc/self/ns/uts", ns::Type::uts);
        if (!joined)
            return child_code(joined.error().code);
        auto uts_joined = ns::id(ns::Type::uts);
        if (!uts_joined)
            return child_id_failed;
        if (*uts_joined != *uts_after)
            return child_wrong_joined_ns;
        return child_ok;
    });
}

TEST_F(TestNs, test_setns_errors)
{
    auto missing = ns::setns("/proc/self/ns/does_not_exist", ns::Type::net);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::not_found);

    auto bad_fd = ns::setns(-1, ns::Type::net);
    ASSERT_FALSE(bad_fd.has_value());
    EXPECT_EQ(bad_fd.error().code, ErrorCode::closed);
}

} // namespace test
