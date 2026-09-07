#ifndef __SIHD_UTIL_MIME_HPP__
#define __SIHD_UTIL_MIME_HPP__

// Well-known mime types, shared across modules. Other modules may add their
// own constants here: an image module would declare the formats it decodes.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::util::mime
{

inline constexpr std::string_view utf8_text = "text/plain;charset=utf-8";
inline constexpr std::string_view plain_text = "text/plain";
inline constexpr std::string_view html = "text/html";
inline constexpr std::string_view uri_list = "text/uri-list";
inline constexpr std::string_view bmp_image = "image/bmp";

// type/subtype match, parameters ignored; empty is "any".
bool compatible(std::string_view a, std::string_view b);

// First offered mime compatible with wanted, else nullopt.
std::optional<std::string_view> match_offered(const std::vector<std::string> & offered, std::string_view wanted);

// First wanted mime with a compatible offer (priority order); returns the wanted
// mime - use match_offered() when the bytes fetched must be the offered mime.
std::optional<std::string_view> best_match(const std::vector<std::string> & offered,
                                           const std::vector<std::string_view> & wanted);

} // namespace sihd::util::mime

#endif
