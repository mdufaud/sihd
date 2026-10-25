#include <windows.h>

#include <fmt/format.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::env
{

std::optional<std::string> get(ZStringView key)
{
    // a 0 return means not found: windows cannot store empty values
    const DWORD size = ::GetEnvironmentVariableA(key.c_str(), nullptr, 0);
    if (size == 0)
        return std::nullopt;
    std::string value(size, '\0');
    const DWORD written = ::GetEnvironmentVariableA(key.c_str(), value.data(), size);
    // a required-size return means the variable changed concurrently: content is undefined
    if (written == 0 || written >= size)
        return std::nullopt;
    value.resize(written);
    return value;
}

Result<void> set(ZStringView key, ZStringView value)
{
    if (::SetEnvironmentVariableA(key.c_str(), value.c_str()) == 0)
    {
        const DWORD err = ::GetLastError();
        const ErrorCode code = err == ERROR_NOT_ENOUGH_MEMORY   ? out_of_memory
                               : err == ERROR_INVALID_PARAMETER ? invalid_argument
                                                                : io_error;
        return std::unexpected(Error(code, "could not set environment variable '{}': {}", key, os::error_str(err)));
    }
    return {};
}

Result<void> unset(ZStringView key)
{
    if (::SetEnvironmentVariableA(key.c_str(), nullptr) == 0)
    {
        const DWORD err = ::GetLastError();
        const ErrorCode code = err == ERROR_NOT_ENOUGH_MEMORY   ? out_of_memory
                               : err == ERROR_INVALID_PARAMETER ? invalid_argument
                                                                : io_error;
        return std::unexpected(Error(code, "could not unset environment variable '{}': {}", key, os::error_str(err)));
    }
    return {};
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
