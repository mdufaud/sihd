
#include <sihd/util/Term.hpp>

#define ESC "\33["

namespace sihd::util
{

const char *Term::Attr::UNDERLINE = ESC "4m";
const char *Term::Attr::BOLD = ESC "1m";
const char *Term::Attr::ITALIC = ESC "3m";
const char *Term::Attr::URL = ESC "4m";
const char *Term::Attr::BLINK = ESC "5m";
const char *Term::Attr::BLINK2 = ESC "6m";
const char *Term::Attr::SELECTED = ESC "7m";

const char *Term::Attr::BLACK = ESC "30m";
const char *Term::Attr::RED = ESC "31m";
const char *Term::Attr::GREEN = ESC "32m";
const char *Term::Attr::YELLOW = ESC "33m";
const char *Term::Attr::BLUE  = ESC "34m";
const char *Term::Attr::VIOLET = ESC "35m";
const char *Term::Attr::CYAN = ESC "36m";
const char *Term::Attr::WHITE = ESC "37m";
const char *Term::Attr::GREY  = ESC "90m";

const char *Term::Attr::BLACKBG = ESC "40m";
const char *Term::Attr::REDBG = ESC "41m";
const char *Term::Attr::GREENBG = ESC "42m";
const char *Term::Attr::YELLOWBG = ESC "43m";
const char *Term::Attr::BLUEBG  = ESC "44m";
const char *Term::Attr::VIOLETBG = ESC "45m";
const char *Term::Attr::CYANBG = ESC "46m";
const char *Term::Attr::WHITEBG = ESC "47m";
const char *Term::Attr::GREYBG = ESC "100m";

const char *Term::Attr::RED2 = ESC "91m";
const char *Term::Attr::GREEN2 = ESC "92m";
const char *Term::Attr::YELLOW2 = ESC "93m";
const char *Term::Attr::BLUE2  = ESC "94m";
const char *Term::Attr::VIOLET2 = ESC "95m";
const char *Term::Attr::CYAN2 = ESC "96m";
const char *Term::Attr::WHITE2 = ESC "97m";

const char *Term::Attr::REDBG2 = ESC "101m";
const char *Term::Attr::GREENBG2 = ESC "102m";
const char *Term::Attr::YELLOWBG2 = ESC "103m";
const char *Term::Attr::BLUEBG2  = ESC "104m";
const char *Term::Attr::VIOLETBG2 = ESC "105m";
const char *Term::Attr::CYANBG2 = ESC "106m";
const char *Term::Attr::WHITEBG2 = ESC "107m";

const char *Term::Attr::ENDC = ESC "0m";

const char *Term::Attr::CLEAR_SCREEN_END = ESC "0J";
const char *Term::Attr::CLEAR_SCREEN_BEG = ESC "1J";
const char *Term::Attr::CLEAR_SCREEN = ESC "2J";

const char *Term::Attr::CLEAR_LINE_END = ESC "0K";
const char *Term::Attr::CLEAR_LINE_BEG = ESC "1K";
const char *Term::Attr::CLEAR_LINE = ESC "2K";

const char *Term::Attr::SAVE_CURSOR = ESC "s";
const char *Term::Attr::RESTORE_CURSOR = ESC "u";

const char *Term::Attr::SCROLL_UP = ESC "S";
const char *Term::Attr::SCROLL_DOWN = ESC "T";

std::string Term::move_cursor_up(int n)
{
    return fmt::format(ESC "{}A", n);
}

std::string Term::move_cursor_down(int n)
{
    return fmt::format(ESC "{}B", n);
}

std::string Term::move_cursor_right(int n)
{
    return fmt::format(ESC "{}C", n);
}

std::string Term::move_cursor_left(int n)
{
    return fmt::format(ESC "{}D", n);
}

std::string Term::next_line(int n)
{
    return fmt::format(ESC "{}E", n);
}

std::string Term::prev_line(int n)
{
    return fmt::format(ESC "{}F", n);
}

std::string Term::set_pos(int line, int col)
{
    return fmt::format(ESC "{};{}H", line, col);
}

std::string Term::set_col(int n)
{
    return fmt::format(ESC "{}G", n);
}

}
