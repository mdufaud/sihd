#include <cstdlib> // getenv setenv unsetenv

#include <sihd/sys/env.hpp>
#include <sihd/util/str.hpp>

extern "C"
{
extern char **environ;
}

namespace sihd::sys::env
{

using namespace sihd::util;

std::optional<std::string> get(std::string_view key)
{
    const char *value = ::getenv(std::string(key).c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
}

bool set(std::string_view key, std::string_view value)
{
    return ::setenv(std::string(key).c_str(), std::string(value).c_str(), 1) == 0;
}

bool unset(std::string_view key)
{
    return ::unsetenv(std::string(key).c_str()) == 0;
}

std::map<std::string, std::string> list()
{
    std::map<std::string, std::string> map;
    for (const char *entry : str::table_span(environ))
    {
        const auto [key, value] = str::split_pair_view(entry, "=");
        if (!key.empty())
            map.emplace(key, value);
    }
    return map;
}

} // namespace sihd::sys::env
