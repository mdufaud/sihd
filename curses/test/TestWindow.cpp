#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/curses/Term.hpp>
#include <sihd/curses/Window.hpp>
#include <sihd/curses/WindowLogger.hpp>
#include <sihd/curses/CursesLogger.hpp>

#include <curses.h>

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

        ASSERT_TRUE(term.is_started());

        Window root("root");
        WindowLogger *win1 = root.add_child<WindowLogger>("win1");
        Window *win2 = root.add_child<Window>("win2");

        root.set_gui_conf({
            .grid_y = 12,
            .grid_x = 12,
        });

        win1->set_gui_conf({
            .grid_y = 12,
            .grid_x = 6,
            .padding = GuiBuilder::Directions::all(1)
        });
        win2->set_gui_conf({
            .grid_y = 12,
            .grid_x = 6,
            // .grid_push = {
            //     .right = 100,
            // },
            .padding = GuiBuilder::Directions::all(1)
        });

        root.init_window();

        win1->set_win_scroll(true);
        win1->set_keypad(true);
        win1->win_border();

        win2->win_write("nolinefeed");
        win2->win_write(" + dual linefeed\n\nafter dual");
        win2->win_write(" !");
        win2->win_write("\nbetween\nafter");
        win2->win_border();

        root.win_refresh();

        while (true)
        {
            auto key = win1->read();
            switch (key)
            {
                case 'q':
                    return ;
                case KEY_UP:
                    win1->win_border_clear();
                    win1->win_scroll(1);
                    win1->cursor_move_y(-1);
                    break ;
                case KEY_DOWN:
                    win1->win_border_clear();
                    win1->win_scroll(-1);
                    win1->cursor_move_y(1);
                    break ;
                case KEY_RESIZE:
                    root.win_resize();
                    win2->win_write("nolinefeed");
                    win2->win_write(" + dual linefeed\n\nafter dual");
                    win2->win_write(" !");
                    win2->win_write("\nbetween\nafter");
                    win2->win_border();
                    break ;
                default:
                {
                    SIHD_LOGF(info, "key = {:d} ({:c})", key, key);
                    break ;
                }
            }
            win1->win_border();
            root.win_refresh();
        }
    }
}