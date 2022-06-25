#include <sihd/curses/Window.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

# include <sihd/util/macro.hpp>

namespace sihd::curses
{

SIHD_UTIL_REGISTER_FACTORY(Window)

SIHD_LOGGER;

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
    if (conf.blocksize_x > sihd::util::GuiBuilder::max_blocksize_x)
    {
        SIHD_LOG(error, "Window: blocksize x cannot be higher than "
            <<  sihd::util::GuiBuilder::max_blocksize_x);
        return false;
    }
    if (conf.blocksize_y > sihd::util::GuiBuilder::max_blocksize_y)
    {
        SIHD_LOG(error, "Window: blocksize y cannot be higher than "
            <<  sihd::util::GuiBuilder::max_blocksize_y);
        return false;
    }
    _gui_conf = conf;
    return true;
}

bool    Window::change_window(const sihd::util::GuiBuilder::Block & pos)
{
    if (mvderwin(_win_ptr, pos.y, pos.x) != OK)
        return false;
    if (wresize(_win_ptr, pos.max_y, pos.max_x) != OK)
        return false;
    _gui_builder.set_window_size(pos.max_y, pos.max_x);
    _gui_builder.clear();
    for (auto & [name, entry]: this->children())
    {
        Window *child = dynamic_cast<Window *>(entry->obj);
        if (child != nullptr)
        {
            auto block = _gui_builder.new_child(child->gui_conf());
            SIHD_LOGF(info, "Changed child {}", child->full_name());
            child->change_window(block);
        }
    }
    return true;
}

bool    Window::init()
{
    if (_win_ptr != nullptr)
        return true;
    Window *parent = this->parent<Window>();
    if (parent != nullptr && parent->is_init() == false)
    {
        SIHD_LOG(error, "Window: must be initialized by parent");
        return false;
    }
    if (parent == nullptr)
        _win_ptr = subwin(stdscr, 0, 0, 0, 0);
    else
        _win_ptr = derwin(parent->window(), 0, 0, 0, 0);
    if (_win_ptr == nullptr)
        return false;
    for (auto & [name, entry]: this->children())
    {
        Window *child = dynamic_cast<Window *>(entry->obj);
        if (child != nullptr)
        {
            if (child->init() == false)
                return false;
        }
    }
    if (parent == nullptr)
    {
        const auto & [max_y, max_x] = Window::stdscr_max_yx();
        this->change_window({
            .y = 0,
            .x = 0,
            .max_y = max_y,
            .max_x = max_x,
        });
    }
    return true;
}

void    Window::read()
{
    int c = wgetch(_win_ptr);
    wprintw(_win_ptr, "lol %c", c);
}

void    Window::write(std::string_view s)
{
    const auto & [y, x] = this->window_yx();
    const auto & [max_y, max_x] = this->window_max_yx();
    const auto & [rel_y, rel_x] = this->window_relative_yx();
    (void)s;
    wprintw(_win_ptr,
        "%s ->"
        " max_y = %d - max_x = %d"
        " - y = %d - x = %d "
        " - rel_y = %d - rel_x = %d", s.data(), max_y, max_x, y, x, rel_y, rel_x);
}

std::pair<int, int> Window::window_relative_yx() const
{
    int y;
    int x;

    getyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::window_cursor_yx() const
{
    int y;
    int x;

    getyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::window_yx() const
{
    int y;
    int x;

    getbegyx(_win_ptr, y, x);
    return {y, x};
}

std::pair<int, int> Window::window_max_yx() const
{
    std::pair<int, int> ret;
    getmaxyx(_win_ptr, ret.first, ret.second);
    return ret;
}

void Window::refresh() const
{
    wrefresh(_win_ptr);
}

void Window::clear() const
{
    wclear(_win_ptr);
}

void Window::erase() const
{
    werase(_win_ptr);
}

std::pair<int, int> Window::stdscr_max_yx()
{
    std::pair<int, int> ret;
    getmaxyx(stdscr, ret.first, ret.second);
    return ret;
}

}