#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Term.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestTerm:   public ::testing::Test
    {
        protected:
            TestTerm()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestTerm()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestTerm, test_term_colors)
    {
        if (Term::is_interactive() == false)
            GTEST_SKIP();
        SIHD_LOG(debug, Term::fmt("BLACK", Term::Attr::BLACK));
        SIHD_LOG(debug, Term::fmt("RED", Term::Attr::RED));
        SIHD_LOG(debug, Term::fmt("GREEN", Term::Attr::GREEN));
        SIHD_LOG(debug, Term::fmt("YELLOW", Term::Attr::YELLOW));
        SIHD_LOG(debug, Term::fmt("BLUE", Term::Attr::BLUE));
        SIHD_LOG(debug, Term::fmt("VIOLET", Term::Attr::VIOLET));
        SIHD_LOG(debug, Term::fmt("CYAN", Term::Attr::CYAN));
        SIHD_LOG(debug, Term::fmt("WHITE", Term::Attr::WHITE));
        SIHD_LOG(debug, Term::fmt("GREY", Term::Attr::GREY));
        SIHD_LOG(debug, Term::fmt("RED2", Term::Attr::RED2));
        SIHD_LOG(debug, Term::fmt("GREEN2", Term::Attr::GREEN2));
        SIHD_LOG(debug, Term::fmt("YELLOW2", Term::Attr::YELLOW2));
        SIHD_LOG(debug, Term::fmt("BLUE2", Term::Attr::BLUE2));
        SIHD_LOG(debug, Term::fmt("VIOLET2", Term::Attr::VIOLET2));
        SIHD_LOG(debug, Term::fmt("CYAN2", Term::Attr::CYAN2));
        SIHD_LOG(debug, Term::fmt("WHITE2", Term::Attr::WHITE2));

        SIHD_LOG(debug, Term::bold("bold"));
        SIHD_LOG(debug, Term::white_bg(" White bg "));
        SIHD_LOG(debug, Term::fmt(" White bg - red text ",
                                    Term::Attr::WHITEBG,
                                    Term::Attr::RED2));

        std::cout << "Testing terminal attrs" << std::endl
            << "======================================================" << std::endl
            << Term::set_col(10) << "10 cols after cursors"
            << std::endl
                << std::endl << "line" << std::endl // making room
                << Term::prev_line(2) << "prev_line"
                << Term::next_line(2) << "next_line"
            << std::endl
                << Term::Attr::SAVE_CURSOR
                << "saved cursor"
                << Term::Attr::RESTORE_CURSOR
                << "restored cursor"
            << std::endl << std::endl // making room
                << Term::move_cursor_up(1)
                << "hello"
                << Term::move_cursor_right(1)
                << Term::move_cursor_down(1)
                << "world"
            << std::endl
                << "i hate you"
                << Term::Attr::CLEAR_LINE
                << Term::move_cursor_left(1000)
                << "i love you"
            << std::endl
                << "garbage xoxo garbage"
                << Term::move_cursor_left(strlen(" garbage"))
                << Term::Attr::CLEAR_LINE_END
                << Term::move_cursor_left(strlen("xoxo") + 1)
                << Term::Attr::CLEAR_LINE_BEG
        << std::endl << "======================================================" << std::endl;
    }
}
