#ifndef __SIHD_NET_IPADDR_HPP__
#define __SIHD_NET_IPADDR_HPP__

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/in.h>
#else
# include <winsock2.h>

# include <ws2ipdef.h>
#endif

namespace sihd::net
{

struct Subnet
{
        unsigned int hosts;
        struct in_addr netid;
        struct in_addr netmask;
        struct in_addr hostmin;
        struct in_addr hostmax;
        struct in_addr broadcast;
        struct in_addr wildcard;
};

class IpAddr
{
    public:
        IpAddr();
        IpAddr(int port, bool ipv6 = false);
        IpAddr(std::string_view host);
        IpAddr(std::string_view host, int port);
        IpAddr(const sockaddr & addr, size_t addr_len);
        IpAddr(const sockaddr & addr);
        IpAddr(const sockaddr_in & addr);
        IpAddr(const sockaddr_in6 & addr);
        IpAddr(const IpAddr & addr);
        IpAddr(IpAddr && addr) = default;
        ~IpAddr() = default;

        IpAddr & operator=(const IpAddr & addr);
        IpAddr & operator=(IpAddr && addr) = default;

        bool operator==(const IpAddr & other) const;

        static IpAddr localhost(int port, bool ipv6 = false);
        // raw in_addr or in6_addr bytes of the family: empty when the size does not match it
        static IpAddr from_bytes(int family, std::span<const uint8_t> bytes);

        void set_hostname(std::string_view hostname);

        bool empty() const;
        bool has_ip() const;
        bool is_ipv4() const;
        bool is_ipv6() const;

        const struct sockaddr & addr() const;
        const struct sockaddr_in & addr4() const;
        const struct sockaddr_in6 & addr6() const;
        std::vector<uint8_t> bytes() const;
        size_t addr_len() const;
        uint16_t port() const;

        sihd::util::Result<void> fetch_hostname();

        std::string str() const;
        const std::string & hostname() const;

        Subnet subnet() const;

        sihd::util::Result<void> set_subnet_mask(const std::string & mask);
        bool set_subnet_mask(uint32_t mask);

        bool is_same_subnet(const IpAddr & other_addr) const;
        bool is_same_subnet(const sockaddr & other_addr) const;
        bool is_same_subnet(const sockaddr_in & other_addr) const;
        bool is_same_subnet(const sockaddr_in6 & other_addr) const;

        bool has_subnet() const;
        uint32_t subnet_value() const;
        std::string dump_subnet() const;

    protected:

    private:
        union SockAddr
        {
                struct sockaddr sockaddr;
                struct sockaddr_in sockaddr_in;
                struct sockaddr_in6 sockaddr_in6;
        };

        std::string _hostname;
        int _port;
        SockAddr _addr;
        uint32_t _netmask_value;
};

} // namespace sihd::net

#endif
