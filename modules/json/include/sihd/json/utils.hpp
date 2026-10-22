#ifndef __SIHD_JSON_UTILS_HPP__
#define __SIHD_JSON_UTILS_HPP__

#include <expected>
#include <functional>
#include <string>
#include <string_view>

#include <sihd/json/fwd.hpp>

namespace sihd::json::utils
{

// absent key is a "key '...' not found" error, anything else a parse error
std::expected<Json, std::string> find_first(std::string_view data, std::string_view key);

// a callback returning false stops the walk early; returns the number of elements walked
std::expected<size_t, std::string> for_each(std::string_view data, std::function<bool(Json)> callback);

} // namespace sihd::json::utils

#endif // __SIHD_JSON_UTILS_HPP__
