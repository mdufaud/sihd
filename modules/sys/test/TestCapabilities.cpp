#include <gtest/gtest.h>

#include <sihd/sys/CapabilitySet.hpp>
#include <sihd/sys/cap.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestCapabilities: public ::testing::Test
{
    protected:
        TestCapabilities() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCapabilities() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!cap::supported)
            {
                GTEST_SKIP() << "capabilities not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestCapabilities, test_capabilities_names)
{
    EXPECT_EQ(cap::all().size(), cap::count);
    EXPECT_EQ(cap::to_name(Cap::net_raw), "cap_net_raw");
    EXPECT_EQ(cap::to_name(Cap::sys_time), "cap_sys_time");

    for (Cap cap : cap::all())
    {
        const std::string_view name = cap::to_name(cap);
        EXPECT_FALSE(name.empty());

        const auto resolved = cap::from_name(name);
        ASSERT_TRUE(resolved.has_value());
        EXPECT_TRUE(*resolved == cap);
    }

    EXPECT_FALSE(cap::from_name("cap_not_a_capability").has_value());
    EXPECT_FALSE(cap::from_name("").has_value());
}

TEST_F(TestCapabilities, test_capabilities_availability)
{
    if constexpr (build::is_windows)
    {
        // only the privileges with a comparable windows effect are mapped
        EXPECT_TRUE(cap::available(Cap::sys_time));
        EXPECT_TRUE(cap::available(Cap::sys_ptrace));
        EXPECT_FALSE(cap::available(Cap::net_raw));
        EXPECT_FALSE(cap::available(Cap::sys_admin));
    }
    else
    {
        for (Cap cap : cap::all())
            EXPECT_TRUE(cap::available(cap));
    }
}

TEST_F(TestCapabilities, test_capabilities_query)
{
    CapabilitySet caps;
    EXPECT_TRUE(caps.refresh());

    // querying own capabilities never requires privileges
    for (Cap cap : cap::all())
    {
        // a capability cannot be effective without being permitted
        if (caps.is_enabled(cap))
        {
            EXPECT_TRUE(caps.permitted(cap));
        }
    }

    for (Cap cap : caps.enabled())
        SIHD_LOG(info, "effective: {}", cap::to_name(cap));

    // a freshly refreshed snapshot must agree with the live state
    for (Cap cap : cap::all())
        EXPECT_EQ(cap::has(cap), caps.is_enabled(cap));
}

TEST_F(TestCapabilities, test_capabilities_raise_not_permitted)
{
    CapabilitySet caps;

    for (Cap cap : cap::all())
    {
        if (caps.permitted(cap))
            continue;
        // raising a capability outside of the permitted set must fail
        EXPECT_FALSE(caps.raise(cap));
        EXPECT_FALSE(caps.is_enabled(cap));
    }
}

TEST_F(TestCapabilities, test_capabilities_raise_permitted)
{
    CapabilitySet caps;

    Cap permitted_cap = Cap::chown;
    bool found = false;
    for (Cap cap : cap::all())
    {
        if (caps.permitted(cap))
        {
            permitted_cap = cap;
            found = true;
            break;
        }
    }
    if (!found)
        GTEST_SKIP() << "process holds no permitted capability";

    // a permitted capability can leave and re-enter the effective set
    EXPECT_TRUE(caps.drop(permitted_cap));
    EXPECT_FALSE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.apply());

    EXPECT_TRUE(caps.refresh());
    EXPECT_FALSE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.permitted(permitted_cap));

    EXPECT_TRUE(caps.raise(permitted_cap));
    EXPECT_TRUE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.apply());

    EXPECT_TRUE(caps.refresh());
    EXPECT_TRUE(caps.is_enabled(permitted_cap));
}

TEST_F(TestCapabilities, test_capabilities_drop)
{
    CapabilitySet caps;

    // dropping is not permanent as long as the capabilities stay permitted: save them to give the
    // rest of the test binary back the state it had
    const std::vector<Cap> saved = caps.enabled();

    // dropping never requires privileges
    caps.drop_all();
    for (Cap cap : cap::all())
        EXPECT_FALSE(caps.is_enabled(cap));

    EXPECT_TRUE(caps.apply());

    // the process really lost them
    EXPECT_TRUE(caps.refresh());
    EXPECT_TRUE(caps.enabled().empty());
    for (Cap cap : saved)
        EXPECT_FALSE(cap::has(cap));

    // restore: the permitted set was untouched, so every saved capability can be raised again
    for (Cap cap : saved)
        EXPECT_TRUE(caps.raise(cap));
    EXPECT_TRUE(caps.apply());

    EXPECT_TRUE(caps.refresh());
    EXPECT_EQ(caps.enabled(), saved);
    for (Cap cap : saved)
        EXPECT_TRUE(cap::has(cap));
}

TEST_F(TestCapabilities, test_capabilities_linux_sets)
{
    CapabilitySet caps;

    if constexpr (build::is_windows)
    {
        // ambient and bounding sets are linux specific
        EXPECT_FALSE(cap::set_ambient(Cap::sys_time, true));
        EXPECT_FALSE(cap::ambient(Cap::sys_time));
        EXPECT_FALSE(cap::bounding(Cap::sys_time));
    }
    else
    {
        // an unprivileged process holds no ambient capability
        EXPECT_FALSE(cap::ambient(Cap::net_raw));
        // setting an ambient capability requires it to be permitted and inheritable
        if (!caps.permitted(Cap::net_raw))
        {
            EXPECT_FALSE(cap::set_ambient(Cap::net_raw, true));
        }
    }
}
} // namespace test
