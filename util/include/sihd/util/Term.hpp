#ifndef __SIHD_UTIL_TERM_HPP__
# define __SIHD_UTIL_TERM_HPP__

# include <stdio.h>
# include <unistd.h>
# include <sihd/util/Str.hpp>

namespace sihd::util
{

class Term
{
    public:

        static bool is_interactive()
        {
            return isatty(fileno(stdin));
        }

        struct Attr
        {
            static const char *UNDERLINE;
            static const char *BOLD;
            static const char *ITALIC;
            static const char *URL;
            static const char *BLINK;
            static const char *BLINK2;
            static const char *SELECTED;

            static const char *BLACK;
            static const char *RED;
            static const char *GREEN;
            static const char *YELLOW;
            static const char *BLUE;
            static const char *VIOLET;
            static const char *CYAN;
            static const char *WHITE;
            static const char *GREY;

            static const char *BLACKBG;
            static const char *REDBG;
            static const char *GREENBG;
            static const char *YELLOWBG;
            static const char *BLUEBG;
            static const char *VIOLETBG;
            static const char *CYANBG;
            static const char *WHITEBG;
            static const char *GREYBG;

            static const char *RED2;
            static const char *GREEN2;
            static const char *YELLOW2;
            static const char *BLUE2;
            static const char *VIOLET2;
            static const char *CYAN2;
            static const char *WHITE2;

            static const char *REDBG2;
            static const char *GREENBG2;
            static const char *YELLOWBG2;
            static const char *BLUEBG2;
            static const char *VIOLETBG2;
            static const char *CYANBG2;
            static const char *WHITEBG2;

            static const char *ENDC;

            // clears from cursor until end of screen
            static const char *CLEAR_SCREEN_END;
            // clears from cursor to beginning of screen
            static const char *CLEAR_SCREEN_BEG;
            // clears entire screen
            static const char *CLEAR_SCREEN;

            // clears from cursor to end of line
            static const char *CLEAR_LINE_END;
            // clears from cursor to start of line
            static const char *CLEAR_LINE_BEG;
            // clears entire line
            static const char *CLEAR_LINE;

            static const char *SAVE_CURSOR;
            static const char *RESTORE_CURSOR;

            static const char *SCROLL_UP;
            static const char *SCROLL_DOWN;
        };

        static std::string fmt(std::string_view str, const char *attr)
        {
            return Str::format("%s%s%s", attr, str.data(), Attr::ENDC);
        }

        static std::string fmt2(std::string_view str, const char *attr, const char *attr2)
        {
            return Str::format("%s%s%s%s", attr, attr2, str.data(), Attr::ENDC);
        }

        static std::string underline(std::string_view str) { return Term::fmt(str, Attr::UNDERLINE); }
        static std::string bold(std::string_view str) { return Term::fmt(str, Attr::BOLD); }
        static std::string selected(std::string_view str) { return Term::fmt(str, Attr::SELECTED); }
        static std::string blink(std::string_view str) { return Term::fmt(str, Attr::BLINK); }

        static std::string red(std::string_view str) { return Term::fmt(str, Attr::RED2); }
        static std::string green(std::string_view str) { return Term::fmt(str, Attr::GREEN2); }

        static std::string white_bg(std::string_view str) { return Term::fmt2(str, Attr::WHITEBG, Attr::BLACK); }

        static std::string move_cursor_up(int n);
        static std::string move_cursor_down(int n);
        static std::string move_cursor_right(int n);
        static std::string move_cursor_left(int n);

        // moves cursor to beginning of line n lines down
        static std::string next_line(int n);
        // moves cursor to beginning of line n lines up
        static std::string prev_line(int n);

        static std::string set_pos(int line, int col);
        static std::string set_col(int n);

    private:
        ~Term() {};
};

}

#endif