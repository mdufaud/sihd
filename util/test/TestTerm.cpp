#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Term.hpp>

namespace test
{
    LOGGER;
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
        LOG(debug, Term::red("Hello world"));
        LOG(debug, Term::bold("bold"));
        LOG(debug, Term::white_bg(" White bg "));
        LOG(debug, Term::fmt2(" White bg - red text ", Term::Attr::WHITEBG, Term::Attr::RED2));
    }
}
