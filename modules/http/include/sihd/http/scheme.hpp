#ifndef __SIHD_HTTP_SCHEME_HPP__
#define __SIHD_HTTP_SCHEME_HPP__

#include <string_view>

#include <sihd/util/ZStringView.hpp>

namespace sihd::http
{

bool scheme_is_ssl(std::string_view scheme);
int scheme_port(sihd::util::ZStringView scheme);

} // namespace sihd::http

#endif
