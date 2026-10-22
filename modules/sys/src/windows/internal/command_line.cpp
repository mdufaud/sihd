#include "command_line.hpp"

namespace sihd::sys::internal
{

std::string command_line(std::span<const std::string_view> args)
{
    std::string line;
    for (std::string_view arg : args)
    {
        if (!line.empty())
            line += ' ';
        if (!arg.empty() && arg.find_first_of(" \t\"") == std::string_view::npos)
        {
            line += arg;
            continue;
        }

        line += '"';
        size_t backslashes = 0;
        for (char c : arg)
        {
            if (c == '\\')
                ++backslashes;
            else
            {
                line.append(c == '"' ? backslashes * 2 + 1 : backslashes, '\\');
                line += c;
                backslashes = 0;
            }
        }
        line.append(backslashes * 2, '\\');
        line += '"';
    }
    return line;
}

} // namespace sihd::sys::internal
