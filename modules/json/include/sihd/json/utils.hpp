#ifndef __SIHD_JSON_UTILS_HPP__
#define __SIHD_JSON_UTILS_HPP__

#include <functional>
#include <optional>
#include <string_view>

#include <sihd/json/fwd.hpp>

namespace sihd::json::utils
{

// early-exit: parses only up to the first top-level object holding key
std::optional<Json> find_first(std::string_view data, std::string_view key);

// walks the top-level array, element by element: returning false from the
// callback stops early
void for_each(std::string_view data, std::function<bool(Json)> callback);

} // namespace sihd::json::utils

#endif // __SIHD_JSON_UTILS_HPP__
