#include <gtest/gtest.h>

#include <sihd/sys/keyboard.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include "../src/linux/internal/desktop_env.hpp"
#endif

namespace test
{

using namespace sihd::util;
using namespace sihd::sys;

TEST(TestKeyboard, test_modifier_combination)
{
    EXPECT_EQ(keyboard::Modifiers::ctrl | keyboard::Modifiers::shift, static_cast<keyboard::Modifiers>(3));
    EXPECT_EQ(keyboard::Modifiers::ctrl & keyboard::Modifiers::shift, keyboard::Modifiers::none);
    EXPECT_EQ(keyboard::Modifiers::ctrl & (keyboard::Modifiers::ctrl | keyboard::Modifiers::alt),
              keyboard::Modifiers::ctrl);
}

// Wine always has a session and SendInput succeeds: sessionless is only provable on linux (clean not_supported)
#if !defined(__SIHD_WINDOWS__)

TEST(TestKeyboard, test_no_session_is_an_error)
{
    if (keyboard::supported())
    {
        if (internal::wayland_session())
            GTEST_SKIP() << "a wayland session runs the injection";
        if (internal::x11_compiled() && getenv("DISPLAY") != nullptr)
            GTEST_SKIP() << "an x11 session runs the injection";
        auto res = keyboard::key_press(keyboard::Key::a);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code, ErrorCode::not_supported);
    }
    else
    {
        GTEST_SKIP() << "no keyboard backend compiled in";
    }
}

#endif

} // namespace test
