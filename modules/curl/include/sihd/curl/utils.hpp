#ifndef __SIHD_CURL_UTILS_HPP__
#define __SIHD_CURL_UTILS_HPP__

#include <string>
#include <string_view>

#include <sihd/util/Result.hpp>

namespace sihd::curl
{

std::string version();

sihd::util::Result<std::string> resolve_url(std::string_view base, std::string_view reference);

} // namespace sihd::curl

#endif
