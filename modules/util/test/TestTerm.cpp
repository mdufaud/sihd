#include <cstdlib>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/term.hpp>

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
class TestTerm: public ::testing::Test
{
    protected:
        TestTerm() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTerm() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTerm, test_supports_color)
{
    // explicit overrides win regardless of the underlying tty state
    unset_env("NO_COLOR");
    set_env("CLICOLOR_FORCE", "1");
    EXPECT_EQ(term::supports_color(), true);

    // NO_COLOR takes precedence over CLICOLOR_FORCE
    set_env("NO_COLOR", "1");
    EXPECT_EQ(term::supports_color(), false);

    unset_env("NO_COLOR");
    unset_env("CLICOLOR_FORCE");
}

TEST_F(TestTerm, test_term_colors)
{
    // the escape sequences are built in memory - no tty needed to verify them
    EXPECT_EQ(term::fmt("text", term::attr::BLACK), "\x1B[30mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::RED), "\x1B[31mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::GREEN), "\x1B[32mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::YELLOW), "\x1B[33mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::BLUE), "\x1B[34mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::VIOLET), "\x1B[35mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::CYAN), "\x1B[36mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::WHITE), "\x1B[37mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::GREY), "\x1B[90mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::RED2), "\x1B[91mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::GREEN2), "\x1B[92mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::YELLOW2), "\x1B[93mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::BLUE2), "\x1B[94mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::VIOLET2), "\x1B[95mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::CYAN2), "\x1B[96mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::WHITE2), "\x1B[97mtext\x1B[0m");

    EXPECT_EQ(term::bold("text"), "\x1B[1mtext\x1B[0m");
    EXPECT_EQ(term::underline("text"), "\x1B[4mtext\x1B[0m");
    EXPECT_EQ(term::selected("text"), "\x1B[7mtext\x1B[0m");
    EXPECT_EQ(term::blink("text"), "\x1B[5mtext\x1B[0m");
    EXPECT_EQ(term::red("text"), "\x1B[91mtext\x1B[0m");
    EXPECT_EQ(term::green("text"), "\x1B[92mtext\x1B[0m");
    EXPECT_EQ(term::white_bg("text"), "\x1B[47m\x1B[30mtext\x1B[0m");
    EXPECT_EQ(term::fmt("text", term::attr::WHITEBG, term::attr::RED2), "\x1B[47m\x1B[91mtext\x1B[0m");

    EXPECT_EQ(term::move_cursor_up(1), "\x1B[1A");
    EXPECT_EQ(term::move_cursor_down(2), "\x1B[2B");
    EXPECT_EQ(term::move_cursor_right(3), "\x1B[3C");
    EXPECT_EQ(term::move_cursor_left(4), "\x1B[4D");
    EXPECT_EQ(term::next_line(1), "\x1B[1E");
    EXPECT_EQ(term::prev_line(2), "\x1B[2F");
    EXPECT_EQ(term::set_pos(3, 4), "\x1B[3;4H");
    EXPECT_EQ(term::set_col(10), "\x1B[10G");

    // the visual rendering can only be eyeballed on a terminal
    if (term::is_interactive())
    {
        SIHD_LOG(debug, "{}", term::fmt("BLACK", term::attr::BLACK));
        SIHD_LOG(debug, "{}", term::fmt("RED", term::attr::RED));
        SIHD_LOG(debug, "{}", term::fmt("GREEN", term::attr::GREEN));
        SIHD_LOG(debug, "{}", term::fmt("YELLOW", term::attr::YELLOW));
        SIHD_LOG(debug, "{}", term::fmt("BLUE", term::attr::BLUE));
        SIHD_LOG(debug, "{}", term::fmt("VIOLET", term::attr::VIOLET));
        SIHD_LOG(debug, "{}", term::fmt("CYAN", term::attr::CYAN));
        SIHD_LOG(debug, "{}", term::fmt("WHITE", term::attr::WHITE));
        SIHD_LOG(debug, "{}", term::fmt("GREY", term::attr::GREY));
        SIHD_LOG(debug, "{}", term::fmt("RED2", term::attr::RED2));
        SIHD_LOG(debug, "{}", term::fmt("GREEN2", term::attr::GREEN2));
        SIHD_LOG(debug, "{}", term::fmt("YELLOW2", term::attr::YELLOW2));
        SIHD_LOG(debug, "{}", term::fmt("BLUE2", term::attr::BLUE2));
        SIHD_LOG(debug, "{}", term::fmt("VIOLET2", term::attr::VIOLET2));
        SIHD_LOG(debug, "{}", term::fmt("CYAN2", term::attr::CYAN2));
        SIHD_LOG(debug, "{}", term::fmt("WHITE2", term::attr::WHITE2));

        SIHD_LOG(debug, "{}", term::bold("bold"));
        SIHD_LOG(debug, "{}", term::white_bg(" White bg "));
        SIHD_LOG(debug, "{}", term::fmt(" White bg - red text ", term::attr::WHITEBG, term::attr::RED2));

        std::cout << "Testing terminal attrs" << std::endl
                  << "======================================================" << std::endl
                  << term::set_col(10) << "10 cols after cursors" << std::endl
                  << std::endl
                  << "line" << std::endl // making room
                  << term::prev_line(2) << "prev_line" << term::next_line(2) << "next_line" << std::endl
                  << term::attr::SAVE_CURSOR << "saved cursor" << term::attr::RESTORE_CURSOR << "restored cursor"
                  << std::endl
                  << std::endl // making room
                  << term::move_cursor_up(1) << "hello" << term::move_cursor_right(1) << term::move_cursor_down(1)
                  << "world" << std::endl
                  << "i hate you" << term::attr::CLEAR_LINE << term::move_cursor_left(1000) << "i love you" << std::endl
                  << "garbage xoxo garbage" << term::move_cursor_left(strlen(" garbage")) << term::attr::CLEAR_LINE_END
                  << term::move_cursor_left(strlen("xoxo") + 1) << term::attr::CLEAR_LINE_BEG << std::endl
                  << "======================================================" << std::endl;
    }
}
} // namespace test
