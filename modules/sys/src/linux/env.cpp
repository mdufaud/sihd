#include <cstdlib>

#include <sihd/sys/env.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

extern "C"
{
extern char **environ;
}

using namespace sihd::util;

namespace sihd::sys::env
{

std::optional<std::string> get(ZStringView key)
{
    const char *value = ::getenv(key.c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
}

Result<void> set(ZStringView key, ZStringView value)
{
    if (::setenv(key.c_str(), value.c_str(), 1) != 0)
        return std::unexpected(Error::from_errno("could not set environment variable '{}'", key));
    return {};
}

Result<void> unset(ZStringView key)
{
    if (::unsetenv(key.c_str()) != 0)
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
