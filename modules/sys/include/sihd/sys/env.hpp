#ifndef __SIHD_SYS_ENV_HPP__
#define __SIHD_SYS_ENV_HPP__

#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/Error.hpp>

namespace sihd::sys::env
{

// mutating the environment is not thread-safe (setenv/unsetenv are not, POSIX)
std::optional<std::string> get(std::string_view key);
std::expected<void, sihd::util::Error> set(std::string_view key, std::string_view value);
std::expected<void, sihd::util::Error> unset(std::string_view key);
std::map<std::string, std::string> list();

// replaces $VAR and ${VAR} with their value; unknown or malformed references are left as-is
std::string expand(std::string_view str);

} // namespace sihd::sys::env

#endif
