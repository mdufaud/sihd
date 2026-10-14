#include <cstdlib>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

namespace
{
void set_env(const char *name, const char *value)
{
#if defined(__SIHD_WINDOWS__)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void unset_env(const char *name)
{
#if defined(__SIHD_WINDOWS__)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}
} // namespace
class TestAnsi: public ::testing::Test
{
    protected:
        TestAnsi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestAnsi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestAnsi, test_color_supported)
{
    // explicit overrides win regardless of the underlying tty state
    unset_env("NO_COLOR");
    set_env("CLICOLOR_FORCE", "1");
    EXPECT_EQ(ansi::color_supported(), true);

    // NO_COLOR takes precedence over CLICOLOR_FORCE
    set_env("NO_COLOR", "1");
    EXPECT_EQ(ansi::color_supported(), false);

    unset_env("NO_COLOR");
    unset_env("CLICOLOR_FORCE");
}

TEST_F(TestAnsi, test_term_colors)
{
    // the escape sequences are built in memory - no tty needed to verify them
    EXPECT_EQ(ansi::fmt("text", ansi::attr::BLACK), "\x1B[30mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::RED), "\x1B[31mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::GREEN), "\x1B[32mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::YELLOW), "\x1B[33mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::BLUE), "\x1B[34mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::VIOLET), "\x1B[35mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::CYAN), "\x1B[36mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::WHITE), "\x1B[37mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::GREY), "\x1B[90mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::RED2), "\x1B[91mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::GREEN2), "\x1B[92mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::YELLOW2), "\x1B[93mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::BLUE2), "\x1B[94mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::VIOLET2), "\x1B[95mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::CYAN2), "\x1B[96mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::WHITE2), "\x1B[97mtext\x1B[0m");

    EXPECT_EQ(ansi::bold("text"), "\x1B[1mtext\x1B[0m");
    EXPECT_EQ(ansi::underline("text"), "\x1B[4mtext\x1B[0m");
    EXPECT_EQ(ansi::selected("text"), "\x1B[7mtext\x1B[0m");
    EXPECT_EQ(ansi::blink("text"), "\x1B[5mtext\x1B[0m");
    EXPECT_EQ(ansi::red("text"), "\x1B[91mtext\x1B[0m");
    EXPECT_EQ(ansi::green("text"), "\x1B[92mtext\x1B[0m");
    EXPECT_EQ(ansi::white_bg("text"), "\x1B[47m\x1B[30mtext\x1B[0m");
    EXPECT_EQ(ansi::fmt("text", ansi::attr::WHITEBG, ansi::attr::RED2), "\x1B[47m\x1B[91mtext\x1B[0m");

    EXPECT_EQ(ansi::move_cursor_up(1), "\x1B[1A");
    EXPECT_EQ(ansi::move_cursor_down(2), "\x1B[2B");
    EXPECT_EQ(ansi::move_cursor_right(3), "\x1B[3C");
    EXPECT_EQ(ansi::move_cursor_left(4), "\x1B[4D");
    EXPECT_EQ(ansi::next_line(1), "\x1B[1E");
    EXPECT_EQ(ansi::prev_line(2), "\x1B[2F");
    EXPECT_EQ(ansi::set_pos(3, 4), "\x1B[3;4H");
    EXPECT_EQ(ansi::set_col(10), "\x1B[10G");

    // the visual rendering can only be eyeballed on a terminal
    if (ansi::color_supported())
    {
        SIHD_LOG(debug, "{}", ansi::fmt("BLACK", ansi::attr::BLACK));
        SIHD_LOG(debug, "{}", ansi::fmt("RED", ansi::attr::RED));
        SIHD_LOG(debug, "{}", ansi::fmt("GREEN", ansi::attr::GREEN));
        SIHD_LOG(debug, "{}", ansi::fmt("YELLOW", ansi::attr::YELLOW));
        SIHD_LOG(debug, "{}", ansi::fmt("BLUE", ansi::attr::BLUE));
        SIHD_LOG(debug, "{}", ansi::fmt("VIOLET", ansi::attr::VIOLET));
        SIHD_LOG(debug, "{}", ansi::fmt("CYAN", ansi::attr::CYAN));
        SIHD_LOG(debug, "{}", ansi::fmt("WHITE", ansi::attr::WHITE));
        SIHD_LOG(debug, "{}", ansi::fmt("GREY", ansi::attr::GREY));
        SIHD_LOG(debug, "{}", ansi::fmt("RED2", ansi::attr::RED2));
        SIHD_LOG(debug, "{}", ansi::fmt("GREEN2", ansi::attr::GREEN2));
        SIHD_LOG(debug, "{}", ansi::fmt("YELLOW2", ansi::attr::YELLOW2));
        SIHD_LOG(debug, "{}", ansi::fmt("BLUE2", ansi::attr::BLUE2));
        SIHD_LOG(debug, "{}", ansi::fmt("VIOLET2", ansi::attr::VIOLET2));
        SIHD_LOG(debug, "{}", ansi::fmt("CYAN2", ansi::attr::CYAN2));
        SIHD_LOG(debug, "{}", ansi::fmt("WHITE2", ansi::attr::WHITE2));

        SIHD_LOG(debug, "{}", ansi::bold("bold"));
        SIHD_LOG(debug, "{}", ansi::white_bg(" White bg "));
        SIHD_LOG(debug, "{}", ansi::fmt(" White bg - red text ", ansi::attr::WHITEBG, ansi::attr::RED2));

        std::cout << "Testing terminal attrs" << std::endl
                  << "======================================================" << std::endl
                  << ansi::set_col(10) << "10 cols after cursors" << std::endl
                  << std::endl
                  << "line" << std::endl // making room
                  << ansi::prev_line(2) << "prev_line" << ansi::next_line(2) << "next_line" << std::endl
                  << ansi::attr::SAVE_CURSOR << "saved cursor" << ansi::attr::RESTORE_CURSOR << "restored cursor"
                  << std::endl
                  << std::endl // making room
                  << ansi::move_cursor_up(1) << "hello" << ansi::move_cursor_right(1) << ansi::move_cursor_down(1)
                  << "world" << std::endl
                  << "i hate you" << ansi::attr::CLEAR_LINE << ansi::move_cursor_left(1000) << "i love you" << std::endl
                  << "garbage xoxo garbage" << ansi::move_cursor_left(strlen(" garbage")) << ansi::attr::CLEAR_LINE_END
                  << ansi::move_cursor_left(strlen("xoxo") + 1) << ansi::attr::CLEAR_LINE_BEG << std::endl
                  << "======================================================" << std::endl;
    }
}
} // namespace test
