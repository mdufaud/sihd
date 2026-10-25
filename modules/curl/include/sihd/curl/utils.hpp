#ifndef __SIHD_CURL_UTILS_HPP__
#define __SIHD_CURL_UTILS_HPP__

#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::curl
{

std::string version();

sihd::util::Result<std::string> resolve_url(sihd::util::ZStringView base, sihd::util::ZStringView reference);

} // namespace sihd::curl

#endif
