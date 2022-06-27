#ifndef __SIHD_CURSES_WINDOW_HPP__
# define __SIHD_CURSES_WINDOW_HPP__

# include <fmt/format.h>

# include <sihd/util/Node.hpp>
# include <sihd/util/GuiBuilder.hpp>
# include <sihd/curses/Term.hpp>

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

        template <typename ...Args>
        void win_write(std::string_view s, Args &&... args)
        {
            std::string fmt = fmt::format(s, std::forward<Args>(args)...);
            return this->_win_write(fmt);
        }

        int read() const;

        bool set_keypad(bool active) const;
        bool set_win_scroll(bool active) const;

        bool cursor_move_x(int cols) const;
        bool cursor_move_y(int lines) const;
        bool win_scroll(int lines) const;
        void win_erase() const;
        void win_refresh() const;
        void win_clear() const;
        void win_border() const;
        void win_border_clear() const;

        void win_resize();

        bool is_window_init() const { return _win_ptr != nullptr; }
        const WINDOW *window() const { return _win_ptr; }
        WINDOW *window() { return _win_ptr; }

        static std::pair<int, int> stdscr_max_yx();
        const sihd::util::GuiBuilder::GuiConf & gui_conf() const { return _gui_conf; }

    protected:
        virtual bool on_add_child(const std::string & name, Named *child) override;
        virtual void on_remove_child(const std::string & name, Named *child) override;

        bool _resize_window(const sihd::util::GuiBuilder::Block & pos);

    private:
        void _win_write(std::string_view s) const;
        void _win_write_padding(std::string_view s) const;
        bool _move_cursors_begin_line(int line) const;

        WINDOW *_win_ptr;

        std::vector<Window *> _subwindows;

        sihd::util::GuiBuilder _gui_builder;
        sihd::util::GuiBuilder::GuiConf _gui_conf;
};

}

#endif