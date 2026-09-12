#include <windows.h> // GetEnvironmentVariableA / SetEnvironmentVariableA / GetEnvironmentStringsA

#include <sihd/sys/env.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys::env
{

using namespace sihd::util;

std::optional<std::string> get(std::string_view key)
{
    const std::string key_str(key);
    // a 0 return means not found: windows cannot store empty values
    const DWORD size = ::GetEnvironmentVariableA(key_str.c_str(), nullptr, 0);
    if (size == 0)
        return std::nullopt;
    std::string value(size, '\0');
    const DWORD written = ::GetEnvironmentVariableA(key_str.c_str(), value.data(), size);
    // a required-size return means the variable changed concurrently: content is undefined
    if (written == 0 || written >= size)
        return std::nullopt;
    value.resize(written);
    return value;
}

bool set(std::string_view key, std::string_view value)
{
    return ::SetEnvironmentVariableA(std::string(key).c_str(), std::string(value).c_str()) != 0;
}

bool unset(std::string_view key)
{
    return ::SetEnvironmentVariableA(std::string(key).c_str(), nullptr) != 0;
}

std::map<std::string, std::string> list()
{
    std::map<std::string, std::string> map;
    // LPCH: the win32 api takes/returns a mutable block even though it must not be mutated
    LPCH block = ::GetEnvironmentStringsA();
    if (block == nullptr)
        return map;
    for (const char *entry = block; *entry != '\0'; entry += std::string_view(entry).size() + 1)
    {
        const auto [key, value] = str::split_pair_view(entry, "=");
        // the block starts with per-drive working directories ("=C:=C:\...") that have no name
        if (!key.empty() && key.front() != '=')
            map.emplace(key, value);
    }
    ::FreeEnvironmentStringsA(block);
    return map;
}

} // namespace sihd::sys::env
