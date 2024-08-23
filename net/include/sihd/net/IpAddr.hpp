#ifndef __SIHD_NET_IPADDR_HPP__
#define __SIHD_NET_IPADDR_HPP__

#include <optional>
#include <vector>

#include <sihd/util/os.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/in.h> // sockaddr
#else
# include <winsock2.h>
# include <ws2ipdef.h> // sockaddr_in6
#endif

namespace sihd::net
{

struct Subnet
{
        // number of hosts
        unsigned int hosts;
        // true network id
        struct in_addr netid;
        // netmask
        struct in_addr netmask;
        // lowest host
        struct in_addr hostmin;
        // highest host
        struct in_addr hostmax;
        // broadcast mask
        struct in_addr broadcast;
        // wildcard mask
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
        ~IpAddr() = default;

        IpAddr & operator=(const IpAddr & addr);

        // build IpAddr from localhost
        static IpAddr localhost(int port, bool ipv6 = false);

        void set_hostname(std::string_view hostname);

        bool empty() const;
        bool has_ip() const;
        bool is_ipv4() const;
        bool is_ipv6() const;

        const struct sockaddr & addr() const;
        const struct sockaddr_in & addr4() const;
        const struct sockaddr_in6 & addr6() const;
        size_t addr_len() const;
        uint16_t port() const;

        bool fetch_hostname();

        std::string str() const;
        const std::string & hostname() const;

        // subnets
        Subnet subnet() const;

        bool set_subnet_mask(std::string_view mask);
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
