#ifndef __SIHD_HTTP_SCHEME_HPP__
#define __SIHD_HTTP_SCHEME_HPP__

#include <string_view>

namespace sihd::http
{

bool scheme_is_ssl(std::string_view scheme);
int scheme_port(std::string_view scheme);

} // namespace sihd::http

#endif
