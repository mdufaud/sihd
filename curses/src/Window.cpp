#include <algorithm>

#include <TextFlow.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/curses/Window.hpp>

#include <curses.h>

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

void    Window::win_resize()
{
    Window *parent = this->parent<Window>();
    if (parent == nullptr)
    {
# ifdef PDCURSES
        resize_term(0, 0);
        erase();
# endif
        this->win_erase();
        _gui_builder.set_window_size({
            .y = 0,
            .x = 0,
            .max_y = LINES,
            .max_x = COLS,
        });
        const auto [max_y, max_x, y, x] = _gui_builder.grid_pos(_gui_conf);
        this->_grid_move_window({
            .y = y,
            .x = x,
            .max_y = max_y,
            .max_x = max_x,
        });
    }
}

bool    Window::_grid_move_window(const sihd::util::GuiBuilder::Block & pos)
{
    _gui_builder.set_window_size(pos);
    const auto grid = _gui_builder.build_grid();
    if (wresize(_win_ptr, pos.max_y, pos.max_x) != OK)
    {
        SIHD_LOGF(error, "Window {} cannot resize to y={} x={}", this->name(), pos.max_y, pos.max_x);
        return false;
    }
    if (mvwin(_win_ptr, pos.y, pos.x) != OK)
    {
        SIHD_LOGF(error, "Window {} cannot move to y={} x={}", this->name(), pos.y, pos.x);
        return false;
    }

    if (grid.empty() == false)
    {
        auto it = grid.begin();
        for (Window *subwindow: _subwindows)
        {
            subwindow->_grid_move_window(*it);
            ++it;
        }
    }
    this->cursor_reset();
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
    this->win_resize();
    return true;
}

bool    Window::_move_cursors_begin_line() const
{
    auto [y, x] = this->win_cursor_yx();
    auto [max_y, _] = this->win_max_yx();
    y = std::max(y, _gui_conf.padding.top);
    x = std::max(x, _gui_conf.padding.left);
    y = std::min(max_y, y);
    return wmove(_win_ptr, y, x) == OK;
}

void    Window::_win_write_padding(std::string_view s) const
{
    size_t curr_pos = 0;
    size_t linefeed_pos = s.find('\n');
    while (linefeed_pos != std::string::npos)
    {
        waddnstr(_win_ptr, s.data() + curr_pos, ((linefeed_pos + 1) - curr_pos));
        this->_move_cursors_begin_line();
        curr_pos = linefeed_pos + 1;
        linefeed_pos = s.find('\n', curr_pos);
    }
    if (curr_pos != s.size())
    {
        waddstr(_win_ptr, s.data() + curr_pos);
    }
}

void    Window::_win_write(std::string_view s) const
{
    auto [_, max_x] = this->win_max_yx();
    int max_width = max_x - _gui_conf.padding.left - _gui_conf.padding.right;
    if (max_width <= 0)
        return ;
    if (s.size() > (size_t)max_width)
    {
        auto str = TextFlow::Column(std::string(s)).width(max_width).toString();
        if (!s.empty() && s.at(s.size() - 1) == '\n')
            str += "\n";
        this->_win_write_padding(str);
    }
    else
    {
        this->_win_write_padding(s);
    }

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
    Window *win = dynamic_cast<Window *>(child);
    if (win != nullptr)
        _subwindows.erase(std::find(_subwindows.begin(), _subwindows.end(), win));
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

bool    Window::cursor_reset() const
{
    return wmove(_win_ptr, _gui_conf.padding.top, _gui_conf.padding.left) == OK;
}

bool    Window::cursor_move_x(int cols) const
{
    auto [y, x] = this->win_cursor_yx();
    return wmove(_win_ptr, y, x + cols) == OK;
}

bool    Window::cursor_move_y(int lines) const
{
    auto [y, x] = this->win_cursor_yx();
    return wmove(_win_ptr, y + lines, x) == OK;
}

bool    Window::win_scroll(int lines) const
{
    return wscrl(_win_ptr, lines) == OK;
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

void    Window::win_border_clear() const
{
    wborder(_win_ptr, ' ', ' ', ' ', ' ' , ' ', ' ', ' ', ' ');
}



bool    Window::set_win_scroll(bool active) const
{
    return scrollok(_win_ptr, active) == OK;
}

bool     Window::set_keypad(bool active) const
{
    // get keys like F1
    return ::keypad(_win_ptr, active) == OK;
}

int     Window::read() const
{
    return wgetch(_win_ptr);
}

std::pair<int, int> Window::stdscr_max_yx()
{
    std::pair<int, int> ret;
    getmaxyx(stdscr, ret.first, ret.second);
    return ret;
}

}