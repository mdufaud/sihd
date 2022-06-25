#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/curses/Term.hpp>
#include <sihd/curses/Window.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");

    using namespace sihd::curses;
    class TestWindow: public ::testing::Test
    {
        protected:
            TestWindow()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestWindow()
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

    TEST_F(TestWindow, test_window)
    {
        Term term;

        Window root("root");
        Window *win1 = root.add_child<Window>("win1");
        Window *win2 = root.add_child<Window>("win2");
        Window *win3 = root.add_child<Window>("win3");

        root.set_gui_conf({
            .blocksize_y = 12,
            .blocksize_x = 12,
        });

        win1->set_gui_conf({
            .blocksize_y = 4,
            .blocksize_x = 12
        });
        win1->set_gui_conf({
            .blocksize_y = 4,
            .blocksize_x = 6
        });
        win1->set_gui_conf({
            .blocksize_y = 4,
            .blocksize_x = 6
        });

        root.init();

        {
            auto [y, x] = win1->window_yx();
            SIHD_LOG_INFO("win1: %d %d", y, x);
        }

        {
            auto [y, x] = win2->window_yx();
            SIHD_LOG_INFO("win2: %d %d", y, x);
        }

        {
            auto [y, x] = win3->window_yx();
            SIHD_LOG_INFO("win3: %d %d", y, x);
        }

        sleep(100);

        win1->write("win1");
        win1->refresh();

        win2->write("win2");
        win2->refresh();

        win3->write("win3");
        win3->refresh();

        sleep(100);
    }
}