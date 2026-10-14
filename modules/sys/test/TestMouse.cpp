#include <gtest/gtest.h>

#include <sihd/sys/mouse.hpp>

#if !defined(__SIHD_WINDOWS__)
# include "../src/linux/internal/desktop_env.hpp"
#endif

namespace test
{

using namespace sihd::util;
using namespace sihd::sys;

// Wine always provides a session and its SendInput succeeds: the sessionless
// case is only provable on linux, where it must be a clean not_supported error.
#if !defined(__SIHD_WINDOWS__)

TEST(TestMouse, test_no_session_is_an_error)
{
    if constexpr (mouse::supported)
    {
        if (internal::wayland_session())
            GTEST_SKIP() << "a wayland session runs the injection";
# if defined(SIHD_COMPILE_WITH_X11)
        if (getenv("DISPLAY") != nullptr)
            GTEST_SKIP() << "an x11 session runs the injection";
# endif
        auto res = mouse::mouse_move(0, 0);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code, ErrorCode::not_supported);
    }
    else
    {
        GTEST_SKIP() << "no mouse backend compiled in";
    }
}

#endif

} // namespace test
