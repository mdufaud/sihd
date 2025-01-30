#ifndef __SIHD_UTIL_TERM_HPP__
#define __SIHD_UTIL_TERM_HPP__

#include <stdio.h>
#include <unistd.h>

#include <string>
#include <string_view>

#define SIHD_TERM_ANSI_UNICODE_ESCAPE "\u001b"
#define SIHD_TERM_ANSI_HEXA_ESCAPE "\x1B"
#define SIHD_TERM_ANSI_DECIMAL_ESCAPE "\27"
#define SIHD_TERM_ANSI_OCTAL_ESCAPE "\033"

#define __ESC SIHD_TERM_ANSI_OCTAL_ESCAPE

namespace sihd::util::term
{

namespace attr
{

inline constexpr const char *UNDERLINE = __ESC "[4m";
inline constexpr const char *BOLD = __ESC "[1m";
inline constexpr const char *ITALIC = __ESC "[3m";
inline constexpr const char *URL = __ESC "[4m";
inline constexpr const char *BLINK = __ESC "[5m";
inline constexpr const char *BLINK2 = __ESC "[6m";
inline constexpr const char *SELECTED = __ESC "[7m";

inline constexpr const char *BLACK = __ESC "[30m";
inline constexpr const char *RED = __ESC "[31m";
inline constexpr const char *GREEN = __ESC "[32m";
inline constexpr const char *YELLOW = __ESC "[33m";
inline constexpr const char *BLUE = __ESC "[34m";
inline constexpr const char *VIOLET = __ESC "[35m";
inline constexpr const char *CYAN = __ESC "[36m";
inline constexpr const char *WHITE = __ESC "[37m";
inline constexpr const char *GREY = __ESC "[90m";

inline constexpr const char *BLACKBG = __ESC "[40m";
inline constexpr const char *REDBG = __ESC "[41m";
inline constexpr const char *GREENBG = __ESC "[42m";
inline constexpr const char *YELLOWBG = __ESC "[43m";
inline constexpr const char *BLUEBG = __ESC "[44m";
inline constexpr const char *VIOLETBG = __ESC "[45m";
inline constexpr const char *CYANBG = __ESC "[46m";
inline constexpr const char *WHITEBG = __ESC "[47m";
inline constexpr const char *GREYBG = __ESC "[100m";

inline constexpr const char *RED2 = __ESC "[91m";
inline constexpr const char *GREEN2 = __ESC "[92m";
inline constexpr const char *YELLOW2 = __ESC "[93m";
inline constexpr const char *BLUE2 = __ESC "[94m";
inline constexpr const char *VIOLET2 = __ESC "[95m";
inline constexpr const char *CYAN2 = __ESC "[96m";
inline constexpr const char *WHITE2 = __ESC "[97m";

inline constexpr const char *REDBG2 = __ESC "[101m";
inline constexpr const char *GREENBG2 = __ESC "[102m";
inline constexpr const char *YELLOWBG2 = __ESC "[103m";
inline constexpr const char *BLUEBG2 = __ESC "[104m";
inline constexpr const char *VIOLETBG2 = __ESC "[105m";
inline constexpr const char *CYANBG2 = __ESC "[106m";
inline constexpr const char *WHITEBG2 = __ESC "[107m";

inline constexpr const char *ENDC = __ESC "[0m";

// clears from cursor until end of screen
inline constexpr const char *CLEAR_SCREEN_END = __ESC "[0J";
// clears from cursor to beginning of screen
inline constexpr const char *CLEAR_SCREEN_BEG = __ESC "[1J";
// clears entire screen
inline constexpr const char *CLEAR_SCREEN = __ESC "[2J";

// clears from cursor to end of line
inline constexpr const char *CLEAR_LINE_END = __ESC "[0K";
// clears from cursor to start of line
inline constexpr const char *CLEAR_LINE_BEG = __ESC "[1K";
// clears entire line
inline constexpr const char *CLEAR_LINE = __ESC "[2K";

inline constexpr const char *SAVE_CURSOR = __ESC "[s";
inline constexpr const char *RESTORE_CURSOR = __ESC "[u";

inline constexpr const char *SCROLL_UP = __ESC "[S";
inline constexpr const char *SCROLL_DOWN = __ESC "[T";

} // namespace attr

void set_output_utf8();

bool is_interactive();

std::string fmt(std::string_view str, const char *attr);
std::string fmt(std::string_view str, const char *attr1, const char *attr2);

std::string underline(std::string_view str);
std::string bold(std::string_view str);
std::string selected(std::string_view str);
std::string blink(std::string_view str);
std::string red(std::string_view str);
std::string green(std::string_view str);
std::string white_bg(std::string_view str);

std::string move_cursor_up(int n);
std::string move_cursor_down(int n);
std::string move_cursor_right(int n);
std::string move_cursor_left(int n);

// moves cursor to beginning of line n lines down
std::string next_line(int n);
// moves cursor to beginning of line n lines up
std::string prev_line(int n);

std::string set_pos(int line, int col);
std::string set_col(int n);

} // namespace sihd::util::term

#undef __ESC

#endif
