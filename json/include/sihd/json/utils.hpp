#ifndef __SIHD_JSON_ONDEMAND_HPP__
#define __SIHD_JSON_ONDEMAND_HPP__

#include <functional>
#include <optional>
#include <string_view>

#include <sihd/json/fwd.hpp>

namespace sihd::json::utils
{

// Parses only until the first object in a top-level array is found, extracts
// the value of `key`, and returns it as a Json. Stops immediately — zero
// allocation for the rest of the document.
std::optional<Json> find_first(std::string_view data, std::string_view key);

// Streams a top-level JSON array element by element, calling callback(Json)
// for each element. Return false from the callback to stop early.
void for_each(std::string_view data, std::function<bool(Json)> callback);

} // namespace sihd::json::utils

#endif
