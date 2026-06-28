#ifndef __SIHD_UTIL_LOCALE_HPP__
#define __SIHD_UTIL_LOCALE_HPP__

#include <locale>
#include <string>
#include <string_view>

namespace sihd::util::locale
{

// Normalize locale name for cross-platform compatibility
// POSIX: en_US.UTF-8 -> Windows: en-US (removes codeset, replaces _ with -)
std::string normalize_locale_name(std::string_view name);

// Create locale from name - throws std::runtime_error if locale not available
std::locale create_locale(std::string_view name);

} // namespace sihd::util::locale

#endif
