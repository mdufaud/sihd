#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

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
    if (term::is_interactive() == false)
        GTEST_SKIP();
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
              << "i hate you" << term::attr::CLEAR_LINE << term::move_cursor_left(1000) << "i love you"
              << std::endl
              << "garbage xoxo garbage" << term::move_cursor_left(strlen(" garbage"))
              << term::attr::CLEAR_LINE_END << term::move_cursor_left(strlen("xoxo") + 1)
              << term::attr::CLEAR_LINE_BEG << std::endl
              << "======================================================" << std::endl;
}
} // namespace test
