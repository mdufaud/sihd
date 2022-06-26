#ifndef __SIHD_CURSES_WINDOW_HPP__
# define __SIHD_CURSES_WINDOW_HPP__

# include <curses.h>
# include <stdarg.h>
# include <fmt/core.h>

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

        bool init_window();

        std::pair<int, int> win_relative_yx() const;
        std::pair<int, int> win_cursor_yx() const;
        std::pair<int, int> win_yx() const;
        std::pair<int, int> win_max_yx() const;

        void win_read();

        template <typename ...Args>
        void win_write(std::string_view s, Args... args)
        {
            std::string fmt = fmt::format(s, args...);
            return this->_win_write(fmt);
        }

        void win_erase() const;
        void win_refresh() const;
        void win_clear() const;
        void win_border() const;

        bool is_window_init() const { return _win_ptr != nullptr; }
        const WINDOW *window() const { return _win_ptr; }
        WINDOW *window() { return _win_ptr; }

        static std::pair<int, int> stdscr_max_yx();

    protected:
        virtual bool on_add_child(const std::string & name, Named *child) override;
        virtual void on_remove_child(const std::string & name, Named *child) override;

        bool resize_window(const sihd::util::GuiBuilder::Block & pos);
        void _win_write(std::string_view s) const;

        const sihd::util::GuiBuilder::GuiConf & gui_conf() const { return _gui_conf; }

    private:
        void _move_cursor_padding(int line) const;

        WINDOW *_win_ptr;

        std::vector<Window *> _subwindows;

        sihd::util::GuiBuilder _gui_builder;
        sihd::util::GuiBuilder::GuiConf _gui_conf;
};

}

#endif