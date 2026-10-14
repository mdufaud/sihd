#include <gtest/gtest.h>

#include <sihd/sys/screen.hpp>

#include "../src/screen_math.hpp"

#if !defined(__SIHD_WINDOWS__)
# include "../src/linux/internal/desktop_env.hpp"
#endif

namespace test
{

using namespace sihd::util;
using namespace sihd::sys;

namespace
{

TEST(TestScreenMath, test_dpi_from)
{
    using namespace sihd::sys::screen::internal;

    // A 1920px panel 508mm wide is exactly 96 dpi, its 4k twin 192.
    EXPECT_NEAR(dpi_from(1920, 508), 96.0, 0.01);
    EXPECT_NEAR(dpi_from(3840, 508), 192.0, 0.01);
    EXPECT_EQ(dpi_from(0, 508), 0.0);
    EXPECT_EQ(dpi_from(1920, 0), 0.0);
    EXPECT_EQ(dpi_from(1920, -1), 0.0);
}

TEST(TestScreen, test_containing_and_primary)
{
    if constexpr (screen::supported)
    {
        auto res = screen::monitors();
#if !defined(__SIHD_WINDOWS__)
        if (!internal::wayland_session())
        {
# if defined(SIHD_COMPILE_WITH_X11)
            if (getenv("DISPLAY") == nullptr)
# endif
            {
                ASSERT_FALSE(res.has_value());
                EXPECT_EQ(res.error().code, ErrorCode::not_supported);
                EXPECT_FALSE(screen::primary().has_value());
                EXPECT_FALSE(screen::containing(0, 0).has_value());
                return;
            }
        }
#endif
        ASSERT_TRUE(res.has_value());
        for (const screen::Monitor & monitor : *res)
        {
            EXPECT_FALSE(monitor.name.empty());
            EXPECT_GT(monitor.width, 0);
            EXPECT_GT(monitor.height, 0);
            EXPECT_GT(monitor.scale, 0.0);
        }
        if (!res->empty())
        {
            const screen::Monitor & first = res->front();
            auto contained = screen::containing(first.x, first.y);
            ASSERT_TRUE(contained.has_value());
            EXPECT_EQ(contained->x, first.x);
            EXPECT_EQ(contained->y, first.y);
        }
        auto primary = screen::primary();
        ASSERT_TRUE(primary.has_value());
        EXPECT_TRUE(primary->primary);
    }
    else
    {
        GTEST_SKIP() << "no screen backend compiled in";
    }
}

} // namespace

} // namespace test
