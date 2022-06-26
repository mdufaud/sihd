#include <sihd/curses/Window.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>
#include <TextFlow.hpp>

namespace sihd::curses
{

SIHD_UTIL_REGISTER_FACTORY(Window)

SIHD_LOGGER;

using namespace sihd::util;

Window::Window(const std::string & name, sihd::util::Node *parent):
    sihd::util::Node(name, parent), _win_ptr(nullptr)
{
}

Window::~Window()
{
    if (_win_ptr != nullptr)
    {
        delwin(_win_ptr);
    }
}

bool    Window::set_gui_conf(const sihd::util::GuiBuilder::GuiConf & conf)
{
    if (conf.grid_x > sihd::util::GuiBuilder::grid_max_x)
    {
        SIHD_LOG(error, "Window: blocksize x cannot be higher than "
            <<  sihd::util::GuiBuilder::grid_max_x);
        return false;
    }
    if (conf.grid_y > sihd::util::GuiBuilder::grid_max_y)
    {
        SIHD_LOG(error, "Window: blocksize y cannot be higher than "
            <<  sihd::util::GuiBuilder::grid_max_y);
        return false;
    }
    _gui_conf = conf;
    return true;
}

bool    Window::resize_window(const sihd::util::GuiBuilder::Block & pos)
{
    // SIHD_LOGF(info, "y={} x={} max_y={} max_x={}", pos.y, pos.x, pos.max_y, pos.max_x);
    _gui_builder.set_window_size(pos);
    const auto grid = _gui_builder.build_grid();
    if (mvwin(_win_ptr, pos.y, pos.x) != OK)
        return false;
    if (wresize(_win_ptr, pos.max_y, pos.max_x) != OK)
        return false;
    if (grid.empty() == false)
    {
        // SIHD_LOGF(info, "{} -> {}", name(), _gui_builder.dump(grid));
        auto it = grid.begin();
        for (Window *subwindow: _subwindows)
        {
            subwindow->resize_window(*it);
            ++it;
        }
    }
    this->_move_cursor_padding(0);
    return true;
}

bool    Window::init_window()
{
    if (_win_ptr != nullptr)
        return true;
    Window *parent = this->parent<Window>();
    if (parent != nullptr && parent->is_window_init() == false)
    {
        SIHD_LOG(error, "Window: parent must be initialized");
        return false;
    }
    if ((_win_ptr = newwin(1, 1, 0, 0)) == nullptr)
    {
        SIHD_LOG(error, "Window: could not allocate window");
        return false;
    }
    _gui_builder.clear_subwindows();
    for (Window *subwindow: _subwindows)
    {
        if (subwindow->init_window() == false)
            return false;
        _gui_builder.add_subwindow(subwindow->gui_conf());
    }
    if (parent == nullptr)
    {
        const auto & [stdscr_max_y, stdscr_max_x] = Window::stdscr_max_yx();
        _gui_builder.set_window_size({
            .y = 0,
            .x = 0,
            .max_y = stdscr_max_y,
            .max_x = stdscr_max_x,
        });
        int max_y = _gui_builder.get_y_from_gridsize(_gui_conf.grid_y);
        int max_x = _gui_builder.get_x_from_gridsize(_gui_conf.grid_x);
        this->resize_window({
            .y = 0,
            .x = 0,
            .max_y = max_y,
            .max_x = max_x,
        });
    }
    return true;
}

void    Window::win_read()
{
    int c = getch();
    win_write("name={} c={}", name(), c);
}

void    Window::_win_write(std::string_view s) const
{
    auto [y, x] = this->win_cursor_yx();
    auto column = TextFlow::Column(s.data()).width(20);
    for (const auto & line: column)
    {
        waddnstr(_win_ptr, line.data(), line.size());
        this->_move_cursor_padding(y);
        ++y;
    }
    /*
    if (s.find("\n") != std::string_view::npos)
    {
        // TODO do not split, find next '\n'
        auto [y, x] = this->win_cursor_yx();
        auto [max_y, max_x] = this->win_max_yx();
        Splitter splitter("\n");
        const auto split = splitter.split_view(s);
        for (const auto split_view: split)
        {
            waddnstr(_win_ptr, split_view.data(), split_view.size());
            this->_move_cursor_padding(y);
            ++y;
        }
    }
    else
    {
        waddnstr(_win_ptr, s.data(), s.size());
    }
    */
}

bool    Window::on_add_child(const std::string & name, Named *child)
{
    (void)name;
    Window *win = dynamic_cast<Window *>(child);
    if (win != nullptr)
        _subwindows.push_back(win);
    return true;
}

void    Window::on_remove_child(const std::string & name, Named *child)
{
    (void)name;
    _subwindows.erase(std::find(_subwindows.begin(), _subwindows.end(), child));
}

std::pair<int, int> Window::win_relative_yx() const
{
    int y;
    int x;

    getyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::win_cursor_yx() const
{
    int y;
    int x;

    getyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::win_yx() const
{
    int y;
    int x;

    getbegyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::win_max_yx() const
{
    std::pair<int, int> ret;
    getmaxyx(_win_ptr, ret.first, ret.second);
    return ret;
}

void    Window::win_refresh() const
{
    wrefresh(_win_ptr);
    for (Window *subwindow: _subwindows)
    {
        subwindow->win_refresh();
    }
}

void    Window::win_clear() const
{
    wclear(_win_ptr);
    for (Window *subwindow: _subwindows)
    {
        subwindow->win_clear();
    }
}

void    Window::win_erase() const
{
    werase(_win_ptr);
    for (Window *subwindow: _subwindows)
    {
        subwindow->win_erase();
    }
}

void    Window::win_border() const
{
    ::wborder(_win_ptr,
        /*left side*/0,
        /*right side*/0,
        /*top side*/0,
        /*bottom side*/0,
        /*top left-hand side*/0,
        /*top right-hand side*/0,
        /*bottom left-hand side*/0,
        /*bottom right-hand side*/0);
}

std::pair<int, int> Window::stdscr_max_yx()
{
    std::pair<int, int> ret;
    getmaxyx(stdscr, ret.first, ret.second);
    return ret;
}

void    Window::_move_cursor_padding(int line) const
{
    wmove(_win_ptr,
        _gui_conf.padding.top + line,
        _gui_conf.padding.left);
}

}