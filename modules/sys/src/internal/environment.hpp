#ifndef __SIHD_SYS_INTERNAL_ENVIRONMENT_HPP__
#define __SIHD_SYS_INTERNAL_ENVIRONMENT_HPP__

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace sihd::sys::internal
{

using EntryLookup = std::function<std::optional<std::string>(std::string_view)>;

std::string expand_entries(std::string_view str, const EntryLookup & lookup);

} // namespace sihd::sys::internal

#endif
