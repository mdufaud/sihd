#include <cstdlib> // getenv setenv unsetenv

#include <sihd/sys/env.hpp>
#include <sihd/util/str.hpp>

extern "C"
{
extern char **environ;
}

using namespace sihd::util;

namespace sihd::sys::env
{

std::optional<std::string> get(std::string_view key)
{
    const char *value = ::getenv(std::string(key).c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
}

std::expected<void, Error> set(std::string_view key, std::string_view value)
{
    if (::setenv(std::string(key).c_str(), std::string(value).c_str(), 1) != 0)
        return std::unexpected(Error::from_errno("could not set environment variable '{}'", key));
    return {};
}

std::expected<void, Error> unset(std::string_view key)
{
    if (::unsetenv(std::string(key).c_str()) != 0)
        return std::unexpected(Error::from_errno("could not unset environment variable '{}'", key));
    return {};
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
