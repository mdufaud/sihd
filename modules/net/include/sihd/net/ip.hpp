#ifndef __SIHD_NET_IP_HPP__
#define __SIHD_NET_IP_HPP__

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

#if defined(__SIHD_WINDOWS__)
// shutdown function corresponding values unix -> windows
# define SHUT_RD SD_RECEIVE
# define SHUT_WR SD_SEND
# define SHUT_RDWR SD_BOTH
// missing socket types
# define SOCK_SEQPACKET 5
# define SOCK_PACKET 10
#endif

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
int protocol(const std::string & name);
int domain(std::string_view name);
int socktype(std::string_view name);

bool is_valid_ipv4(const std::string & ip);
bool is_valid_ipv6(const std::string & ip);
bool is_valid_ip(const std::string & ip);

bool is_private_ipv4(uint32_t addr);
// not_found when the host does not resolve
sihd::util::Result<bool> is_private_host(const std::string & host);
// permission_denied when one address is private, not_found when unresolved
sihd::util::Result<std::vector<std::string>> public_addresses(const std::string & host);

std::string to_str(const sockaddr *addr);
std::string to_str(const sockaddr_in *addr_in);
std::string to_str(const sockaddr_in6 *addr_in);
std::string to_str(const in_addr *addr_in);
std::string to_str(const in6_addr *addr_in);

// from netmask value number to network netmask
uint32_t to_netmask(uint32_t value);
bool is_valid_netmask(uint32_t mask);

} // namespace sihd::net::ip

#endif
