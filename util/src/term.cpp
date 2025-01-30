#include <fmt/format.h>

#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
#endif

namespace sihd::util::term
{

void set_output_utf8()
{
#if defined(__SIHD_WINDOWS__)
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif
}

std::string fmt(std::string_view str, const char *attr)
{
    return fmt::format("{}{}{}", attr, str, attr::ENDC);
}

std::string fmt(std::string_view str, const char *attr1, const char *attr2)
{
    return fmt::format("{}{}{}{}", attr1, attr2, str, attr::ENDC);
}

bool is_interactive()
{
    return isatty(fileno(stdin));
}

std::string underline(std::string_view str)
{
    return term::fmt(str, attr::UNDERLINE);
}

std::string bold(std::string_view str)
{
    return term::fmt(str, attr::BOLD);
}

std::string selected(std::string_view str)
{
    return term::fmt(str, attr::SELECTED);
}

std::string blink(std::string_view str)
{
    return term::fmt(str, attr::BLINK);
}

std::string red(std::string_view str)
{
    return term::fmt(str, attr::RED2);
}

std::string green(std::string_view str)
{
    return term::fmt(str, attr::GREEN2);
}

std::string white_bg(std::string_view str)
{
    return term::fmt(str, attr::WHITEBG, attr::BLACK);
}

std::string move_cursor_up(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}A", n);
}

std::string move_cursor_down(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}B", n);
}

std::string move_cursor_right(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}C", n);
}

std::string move_cursor_left(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}D", n);
}

std::string next_line(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}E", n);
}

std::string prev_line(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}F", n);
}

std::string set_pos(int line, int col)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{};{}H", line, col);
}

std::string set_col(int n)
{
    return fmt::format(SIHD_TERM_ANSI_OCTAL_ESCAPE "[{}G", n);
}

} // namespace sihd::util::term
