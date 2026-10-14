#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/keyboard.hpp>

#if !defined(__SIHD_WINDOWS__)
# include "../src/linux/internal/desktop_env.hpp"
#endif

namespace test
{

using namespace sihd::util;
using namespace sihd::sys;

namespace
{

// Injection lands in the focused window of a live session: only the opt-in
// runs (SIHD_TEST_INPUT=1) may type into the developer's desktop.
bool input_enabled()
{
    const char *enabled = getenv("SIHD_TEST_INPUT");
    return enabled != nullptr && std::string(enabled) == "1";
}

TEST(TestKeyboard, test_modifier_combination)
{
    EXPECT_EQ(keyboard::Modifiers::ctrl | keyboard::Modifiers::shift, static_cast<keyboard::Modifiers>(3));
    EXPECT_EQ(keyboard::Modifiers::ctrl & keyboard::Modifiers::shift, keyboard::Modifiers::none);
    EXPECT_EQ(keyboard::Modifiers::ctrl & (keyboard::Modifiers::ctrl | keyboard::Modifiers::alt),
              keyboard::Modifiers::ctrl);
}

// Wine always provides a session and its SendInput succeeds: the sessionless
// case is only provable on linux, where it must be a clean not_supported error.
#if !defined(__SIHD_WINDOWS__)

TEST(TestKeyboard, test_no_session_is_an_error)
{
    if constexpr (keyboard::supported)
    {
        if (internal::wayland_session())
            GTEST_SKIP() << "a wayland session runs the injection";
# if defined(SIHD_COMPILE_WITH_X11)
        if (getenv("DISPLAY") != nullptr)
            GTEST_SKIP() << "an x11 session runs the injection";
# endif
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

TEST(TestKeyboard, test_active_injection)
{
    if constexpr (!keyboard::supported)
    {
        GTEST_SKIP() << "no keyboard backend compiled in";
    }
    if (!input_enabled())
    {
        GTEST_SKIP() << "set SIHD_TEST_INPUT=1 to type into the live session";
    }

    EXPECT_TRUE(keyboard::type_text("sihd").has_value());
    EXPECT_TRUE(keyboard::key_press(keyboard::Key::enter).has_value());
    EXPECT_TRUE(keyboard::key_press(keyboard::Key::c, keyboard::Modifiers::ctrl).has_value());
    EXPECT_TRUE(keyboard::key_down(keyboard::Key::a).has_value());
    EXPECT_TRUE(keyboard::key_up(keyboard::Key::a).has_value());
    EXPECT_FALSE(keyboard::type_text("\xff\xfe").has_value());
    EXPECT_FALSE(keyboard::key_press(keyboard::Key::none).has_value());
}

} // namespace

} // namespace test
