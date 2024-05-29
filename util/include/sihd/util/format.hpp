#ifndef __SIHD_UTIL_FMT_HPP__
#define __SIHD_UTIL_FMT_HPP__

#include <fmt/format.h>

namespace sihd::util::format
{

template <typename... Args>
std::string join(std::string_view sep, Args &&...args)
{
    constexpr char brace[] = "{}";
    constexpr int brace_size = sizeof(brace) - 1;

    if (sizeof...(Args) == 0)
        return "";

    // fun(sep, "a", "b", "c") -> {}sep{}sep{} -> a,b,c
    const size_t number_of_sep = sizeof...(Args) - 1;
    const size_t size = (sizeof...(Args) * brace_size) + (number_of_sep * sep.size());
    char format_str[size + 1];

    int i = 0;
    size_t current_arg = 0;
    while (current_arg < sizeof...(Args))
    {
        if (current_arg > 0)
        {
            strcpy(format_str + i, sep.data());
            i += sep.size();
        }
        strcpy(format_str + i, brace);
        i += brace_size;
        ++current_arg;
    }

    format_str[size] = 0;

    return fmt::format(std::string_view(format_str, size), std::forward<Args>(args)...);
};

} // namespace sihd::util::format

#endif
