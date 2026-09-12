#include <sihd/sys/env.hpp>

#include "internal/environment.hpp"

namespace sihd::sys::env
{

std::string expand(std::string_view str)
{
    return internal::expand_entries(str, [](std::string_view key) { return get(key); });
}

} // namespace sihd::sys::env
