#ifndef __SIHD_NET_IP_HPP__
#define __SIHD_NET_IP_HPP__

#include <string_view>

namespace sihd::net::ip
{

const char *domain_str(int domain);
const char *socktype_str(int socktype);
const char *protocol_str(int protocol);
int protocol(std::string_view name);
int domain(std::string_view name);
int socktype(std::string_view name);

} // namespace sihd::net::ip

#endif
