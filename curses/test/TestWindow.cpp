#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/curses/Term.hpp>
#include <sihd/curses/Window.hpp>
#include <sihd/curses/CursesLogger.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");

    using namespace sihd::util;
    using namespace sihd::curses;

    class TestWindow: public ::testing::Test
    {
        protected:
            TestWindow()
            {
            }

            virtual ~TestWindow()
            {
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
        TmpLogger tmp(new CursesLogger(stdscr));

        Window root("root");
        Window *win1 = root.add_child<Window>("win1");
        Window *win2 = root.add_child<Window>("win2");
        Window *win3 = root.add_child<Window>("win3");

        root.set_gui_conf({
            .grid_y = 12,
            .grid_x = 12,
        });

        win1->set_gui_conf({
            .grid_y = 4,
            .grid_x = 12,
            .padding = {
                .left = 1,
                .right = 1,
                .top = 1,
                .bottom = 1,
            }
        });
        win2->set_gui_conf({
            .grid_y = 4,
            .grid_x = 5,
            .grid_push = {
                .right = 3,
            },
            .padding = {
                .left = 1,
                .right = 1,
                .top = 1,
                .bottom = 1,
            }
        });
        win3->set_gui_conf({
            .grid_y = 4,
            .grid_x = 5,
            /*
            .grid_push = {
                .left = 1,
            },
            */
            .margin = {
                .left = 1,
                .top = 1,
                .bottom = 1,
            },
            .padding = {
                .left = 1,
                .right = 1,
                .top = 1,
                .bottom = 1,
            }
        });

        root.init_window();

        win1->win_write("win1 {}\n", 20);
        win1->win_write("blbalblalbalblalballbalblalbalblalblalblalbalblalbalb {}\n", 1);
        win1->win_write("win1 {} -", 2);
        win1->win_write(" win1 {}\n", 3);
        win1->win_border();

        win2->win_write("win2");
        win2->win_write("win2\n\n");
        win2->win_write("win2");
        win2->win_write(" win2");
        win2->win_write("---");
        win2->win_border();

        win3->win_write("win3");
        win3->win_border();

        root.win_refresh();

        sleep(100);
    }
}