#include <cstdlib>
#include <cstring>

#include <fmt/format.h>

#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

namespace sihd::util::ansi
{

std::string fmt(std::string_view str, const char *attr)
{
    return fmt::format("{}{}{}", attr, str, attr::ENDC);
}

std::string fmt(std::string_view str, const char *attr1, const char *attr2)
{
    return fmt::format("{}{}{}{}", attr1, attr2, str, attr::ENDC);
}

bool color_supported(FILE *output)
{
    // explicit overrides (https://no-color.org, https://bixense.com/clicolors)
    if (::getenv("NO_COLOR") != nullptr)
        return false;
    const char *clicolor_force = ::getenv("CLICOLOR_FORCE");
    if (clicolor_force != nullptr && clicolor_force[0] != '0' && clicolor_force[0] != '\0')
        return true;
    if constexpr (build::is_emscripten)
    {
        return false;
    }
    else if constexpr (build::is_android)
    {
        // The terminal bridge UI renders ANSI escapes, so color is supported even
        // though stdout/stderr is a pipe (not a tty).
        return true;
    }
    else
    {
        if (!isatty(fileno(output)))
            return false;
        const char *term_env = ::getenv("TERM");

        if (term_env == nullptr || std::strcmp(term_env, "dumb") == 0)
            return false;
        return true;
    }
}

std::string underline(std::string_view str)
{
    return fmt(str, attr::UNDERLINE);
}

std::string bold(std::string_view str)
{
    return fmt(str, attr::BOLD);
}

std::string selected(std::string_view str)
{
    return fmt(str, attr::SELECTED);
}

std::string blink(std::string_view str)
{
    return fmt(str, attr::BLINK);
}

std::string red(std::string_view str)
{
    return fmt(str, attr::RED2);
}

std::string green(std::string_view str)
{
    return fmt(str, attr::GREEN2);
}

std::string white_bg(std::string_view str)
{
    return fmt(str, attr::WHITEBG, attr::BLACK);
}

std::string move_cursor_up(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}A", n);
}

std::string move_cursor_down(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}B", n);
}

std::string move_cursor_right(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}C", n);
}

std::string move_cursor_left(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}D", n);
}

std::string next_line(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}E", n);
}

std::string prev_line(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}F", n);
}

std::string set_pos(int line, int col)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{};{}H", line, col);
}

std::string set_col(int n)
{
    return fmt::format(SIHD_ANSI_OCTAL_ESCAPE "[{}G", n);
}

} // namespace sihd::util::ansi
