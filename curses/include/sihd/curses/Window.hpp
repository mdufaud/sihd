#ifndef __SIHD_CURSES_WINDOW_HPP__
# define __SIHD_CURSES_WINDOW_HPP__

# include <curses.h>

# include <sihd/util/Node.hpp>
# include <sihd/util/GuiBuilder.hpp>

namespace sihd::curses
{

class Window:   public sihd::util::Node
{
    public:
        Window(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~Window();

        bool set_gui_conf(const sihd::util::GuiBuilder::GuiConf & conf);

        bool init();

        static std::pair<int, int> stdscr_max_yx();

        std::pair<int, int> window_relative_yx() const;
        std::pair<int, int> window_cursor_yx() const;
        std::pair<int, int> window_yx() const;
        std::pair<int, int> window_max_yx() const;

        void read();
        void write(std::string_view s);
        void erase() const;
        void refresh() const;
        void clear() const;

        bool is_init() const { return _win_ptr != nullptr; }
        const WINDOW *window() const { return _win_ptr; }
        WINDOW *window() { return _win_ptr; }

    protected:
        bool change_window(const sihd::util::GuiBuilder::Block & pos);
        const sihd::util::GuiBuilder::GuiConf & gui_conf() const { return _gui_conf; }

    private:
        WINDOW *_win_ptr;
        sihd::util::GuiBuilder _gui_builder;
        sihd::util::GuiBuilder::GuiConf _gui_conf;
};

}

#endif