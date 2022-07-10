#include <sihd/util/Logger.hpp>
#include <sihd/curses/Curses.hpp>
#include <sihd/curses/Window.hpp>
#include <sihd/curses/WindowLogger.hpp>
#include <sihd/curses/CursesLogger.hpp>

#include <curses.h>

SIHD_NEW_LOGGER("main")

using namespace sihd::util;
using namespace sihd::curses;

int main()
{
    Curses curses;

    if (curses.is_started() == false)
        return 1;

    Window root("root");
    Window *win1 = root.add_child<Window>("win1");
    WindowLogger *win2 = root.add_child<WindowLogger>("win2");

    root.set_gui_conf({
        .grid_y = 12,
        .grid_x = 12,
    });

    win1->set_gui_conf({
        .grid_y = 12,
        .grid_x = 6,
        .padding = {1}
    });
    win2->set_gui_conf({
        .grid_y = 12,
        .grid_x = 6,
        // .grid_push = {
        //     .right = 100,
        // },
        .padding = {1}
    });

    root.init_window();

    win2->set_win_scroll(true);
    win2->set_keypad(true);
    win2->win_border();

    win1->win_write("line1");
    win1->win_write(" + still line1\n\nline3");
    win1->win_write(" !");
    win1->win_write("\nline4\nline5\n");
    win1->win_write("line6");
    win1->win_border();

    root.win_refresh();

    while (true)
    {
        auto key = win2->read();
        switch (key)
        {
            case 'q':
                return 0;
            case KEY_UP:
                win2->win_border_clear();
                win2->win_scroll(1);
                win2->cursor_move_y(-1);
                break ;
            case KEY_DOWN:
                win2->win_border_clear();
                win2->win_scroll(-1);
                win2->cursor_move_y(1);
                break ;
            case KEY_RESIZE:
                root.win_resize();
                win1->win_write("line1");
                win1->win_write(" + still line1\n\nline3");
                win1->win_write(" !");
                win1->win_write("\nline4\nline5\n");
                win1->win_write("line6");
                win1->win_border();
                break ;
            default:
            {
                SIHD_LOGF(info, "key = {} ({})", key, (char)key);
                break ;
            }
        }
        win2->win_border();
        root.win_refresh();
    }
    return 0;
}