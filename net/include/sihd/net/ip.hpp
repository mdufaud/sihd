#ifndef __SIHD_NET_IP_HPP__
#define __SIHD_NET_IP_HPP__

#include <string_view>

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;
struct in_addr;
struct in6_addr;

namespace sihd::net::ip
{

const char *domain_str(int domain);
const char *socktype_str(int socktype);
const char *protocol_str(int protocol);
int protocol(std::string_view name);
int domain(std::string_view name);
int socktype(std::string_view name);

bool is_valid_ipv4(std::string_view ip);
bool is_valid_ipv6(std::string_view ip);
bool is_valid_ip(std::string_view ip);

std::string to_str(const sockaddr *addr);
std::string to_str(const sockaddr_in *addr_in);
std::string to_str(const sockaddr_in6 *addr_in);
std::string to_str(const in_addr *addr_in);
std::string to_str(const in6_addr *addr_in);

} // namespace sihd::net::ip

#endif
