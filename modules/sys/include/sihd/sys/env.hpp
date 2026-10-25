#ifndef __SIHD_SYS_ENV_HPP__
#define __SIHD_SYS_ENV_HPP__

#include <map>
#include <optional>
#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::env
{

// mutating the environment is not thread-safe (setenv/unsetenv are not, POSIX)
std::optional<std::string> get(sihd::util::ZStringView key);
sihd::util::Result<void> set(sihd::util::ZStringView key, sihd::util::ZStringView value);
sihd::util::Result<void> unset(sihd::util::ZStringView key);
std::map<std::string, std::string> list();

// replaces $VAR and ${VAR} with their value; unknown or malformed references are left as-is
std::string expand(std::string_view str);

} // namespace sihd::sys::env

#endif
