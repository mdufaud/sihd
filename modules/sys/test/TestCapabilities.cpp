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
        TestCapabilities() { LoggerManager::stream(); }

        virtual ~TestCapabilities() { LoggerManager::clear_loggers(); }

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
    EXPECT_TRUE(caps.refresh().has_value());

    for (Cap cap : cap::all())
    {
        if (caps.is_enabled(cap))
        {
            EXPECT_TRUE(caps.permitted(cap));
        }
    }

    for (Cap cap : caps.enabled())
        SIHD_LOG(info, "effective: {}", cap::to_name(cap));

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
        EXPECT_FALSE(caps.raise(cap));
        EXPECT_FALSE(caps.is_enabled(cap));
    }
}

TEST_F(TestCapabilities, test_capabilities_raise_permitted_as_root)
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

    EXPECT_TRUE(caps.drop(permitted_cap));
    EXPECT_FALSE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.apply().has_value());

    EXPECT_TRUE(caps.refresh().has_value());
    EXPECT_FALSE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.permitted(permitted_cap));

    EXPECT_TRUE(caps.raise(permitted_cap));
    EXPECT_TRUE(caps.is_enabled(permitted_cap));
    EXPECT_TRUE(caps.apply().has_value());

    EXPECT_TRUE(caps.refresh().has_value());
    EXPECT_TRUE(caps.is_enabled(permitted_cap));
}

TEST_F(TestCapabilities, test_capabilities_drop)
{
    CapabilitySet caps;

    // dropping is not permanent while permitted: save the set to give the rest of the binary its state back
    const std::vector<Cap> saved = caps.enabled();

    caps.drop_all();
    for (Cap cap : cap::all())
        EXPECT_FALSE(caps.is_enabled(cap));

    EXPECT_TRUE(caps.apply().has_value());

    EXPECT_TRUE(caps.refresh().has_value());
    EXPECT_TRUE(caps.enabled().empty());
    for (Cap cap : saved)
        EXPECT_FALSE(cap::has(cap));

    for (Cap cap : saved)
        EXPECT_TRUE(caps.raise(cap));
    EXPECT_TRUE(caps.apply().has_value());

    EXPECT_TRUE(caps.refresh().has_value());
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
        EXPECT_FALSE(cap::ambient(Cap::net_raw));
        if (!caps.permitted(Cap::net_raw))
        {
            EXPECT_FALSE(cap::set_ambient(Cap::net_raw, true));
        }
    }
}
} // namespace test
