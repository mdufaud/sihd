#ifndef __SIHD_NET_IPADDR_HPP__
#define __SIHD_NET_IPADDR_HPP__

#include <optional>
#include <vector>

#include <sihd/util/os.hpp>

#include <sihd/net/ip.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/in.h> // sockaddr
#else
# include <winsock2.h>
# include <ws2ipdef.h> // sockaddr_in6
#endif

struct addrinfo; // in #include <netdb.h>

namespace sihd::net
{

// permits easy ipv4 / ipv6 manipulation
struct IpSockAddr
{
        int type;
        socklen_t addr_len;
        sockaddr *addr;
        sockaddr_in addr_in;
        sockaddr_in6 addr_in6;
};

class IpAddr
{
    public:
        IpAddr();
        IpAddr(int port, bool ipv6 = false);
        IpAddr(std::string_view host, bool dns_lookup = false);
        IpAddr(std::string_view host, int port, bool dns_lookup = false);
        IpAddr(const sockaddr & addr, size_t addr_len, bool dns_lookup = false);
        IpAddr(const sockaddr & addr, bool dns_lookup = false);
        IpAddr(const sockaddr_in & addr, bool dns_lookup = false);
        IpAddr(const sockaddr_in6 & addr, bool dns_lookup = false);

        virtual ~IpAddr();

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

        struct IpEntry
        {
                bool ipv6;
                int socktype;
                int protocol;
                in_addr addr;
                in6_addr addr6;

                std::string ip() const { return ipv6 ? IpAddr::ip_str(this->addr6) : IpAddr::ip_str(this->addr); }
        };

        // dns lookup
        struct DnsInfo
        {
                std::string hostname;
                std::vector<IpEntry> lst_ip;
        };

        // do a DNS lookup to find every ip addr for every socket and every protocols
        static std::optional<DnsInfo> dns_lookup(std::string_view host, bool ipv6 = false);
        static std::string fetch_ip_name(std::string_view ip);
        static std::string fetch_ip_name(sockaddr *addr, socklen_t addr_len);

        // checks both ipv4 and ipv6
        static bool is_valid_ip(std::string_view ip);
        static bool is_valid_ipv4(std::string_view ip);
        static bool is_valid_ipv6(std::string_view ip);
        static std::string ip_str(const sockaddr & addr);
        static std::string ipv4_str(const sockaddr & addr);
        static std::string ipv6_str(const sockaddr & addr);
        static std::string ip_str(const sockaddr_in & addr_in);
        static std::string ip_str(const sockaddr_in6 & addr_in);
        static std::string ip_str(const in_addr & addr_in);
        static std::string ip_str(const in6_addr & addr_in);

        /*
            depending on ip, fills ipsockaddr addr, addr_len and type with corresponding ipv4 or ipv6 struct
            port is optionnal and setted in addr_in or addr_in6
        */
        static bool fill_ipsockaddr(IpSockAddr & ipsockaddr, std::string_view ip, int port = 0);
        // fills sockaddr_in from ip/port
        static bool to_sockaddr_in(sockaddr_in *filled, std::string_view ip, int port = 0);
        // fills sockaddr_in6 from ip(v6)/port
        static bool to_sockaddr_in6(sockaddr_in6 *filled, std::string_view ip, int port = 0);
        // fills sockaddr_in from ip/port
        static bool to_sockaddr_in(sockaddr_in *filled, const sockaddr & addr, size_t addr_len);
        // fills sockaddr_in6 from ip(v6)/port
        static bool to_sockaddr_in6(sockaddr_in6 *filled, const sockaddr & addr, size_t addr_len);

        // build IpAddr from localhost
        static IpAddr localhost(int port, bool ipv6 = false);

        // apply mask on src to dst to get netid addr
        static void fill_sockaddr_network_id(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask);
        static void fill_sockaddr_network_id(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask);
        // apply mask on src to dst to get broadcast addr
        static void fill_sockaddr_broadcast(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask);
        static void fill_sockaddr_broadcast(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask);
        // transform value to network mask
        static uint32_t to_netmask(uint32_t value);
        // check for valid netmask
        static bool is_valid_netmask(uint32_t mask);

        // remove any ip info
        void clear();

        // subnets
        bool is_same_subnet(std::string_view ip) const;
        bool is_same_subnet(const sockaddr_in & addr) const;
        bool is_same_subnet(const IpAddr & addr) const;

        bool set_subnet_mask(std::string_view mask);
        bool set_subnet_mask(uint32_t mask);
        bool has_subnet() const { return _has_subnet; }
        std::string dump_subnet() const;
        const Subnet & subnet() const { return _subnet; }
        size_t subnet_value() const;

        // clear and set ip from source
        void from_any(int port, bool ipv6 = false);
        bool from(std::string_view host, int port = 0);
        bool from(const sockaddr & addr, size_t addr_len);
        bool from(const sockaddr_in & addr_in);
        bool from(const sockaddr_in6 & addr_in6);

        void set_ipv6_preference(bool active) { _prefers_ipv6 = active; }
        bool prefers_ipv6() const { return _prefers_ipv6; }

        // do a dns_lookup and fill internal object
        bool do_lookup_dns();

        // returns ip sock address for socket type and protocol
        bool get_sockaddr(IpSockAddr & ipsockaddr, int socktype, int protocol) const;
        bool get_sockaddr_in(sockaddr_in *filled, int socktype, int protocol) const;
        bool get_sockaddr_in6(sockaddr_in6 *filled, int socktype, int protocol) const;
        bool get_first_sockaddr(IpSockAddr & ipsockaddr) const { return this->get_sockaddr(ipsockaddr, -1, -1); };
        bool get_first_sockaddr_in(sockaddr_in *filled) const { return this->get_sockaddr_in(filled, -1, -1); }
        bool get_first_sockaddr_in6(sockaddr_in6 *filled) const { return this->get_sockaddr_in6(filled, -1, -1); }

        // returns string ip address
        std::string matching_ip_str(int socktype, int protocol, bool ipv6 = false) const;
        // returns string ip address for socket type
        std::string socktype_ip_str(int socktype, bool ipv6 = false) const
        {
            return this->matching_ip_str(socktype, -1, ipv6);
        }
        // returns string ip address for protocol
        std::string protocol_ip_str(int protocol, bool ipv6 = false) const
        {
            return this->matching_ip_str(-1, protocol, ipv6);
        }

        // returns string ip address
        std::string matching_ip_str(std::string_view socktype, std::string_view protocol, bool ipv6 = false) const;
        // returns string ip address for socket type
        std::string socktype_ip_str(std::string_view socktype, bool ipv6 = false) const
        {
            return this->matching_ip_str(socktype, "", ipv6);
        }
        // returns string ip address for protocol
        std::string protocol_ip_str(std::string_view protocol, bool ipv6 = false) const
        {
            return this->matching_ip_str("", protocol, ipv6);
        }

        std::string first_ipv4_str() const { return this->matching_ip_str(-1, -1, false); }
        std::string first_ipv6_str() const { return this->matching_ip_str(-1, -1, true); }
        std::string first_ip_str() const
        {
            return this->prefers_ipv6() ? this->first_ipv6_str() : this->first_ipv4_str();
        }

        std::string dump_ip_lst() const;

        size_t ip_count() const { return _lst_ip.size(); }
        size_t ipv4_count() const;
        size_t ipv6_count() const;

        // getters
        const std::string & host() const { return _host; }
        const std::string & hostname() const { return _hostname; }
        int port() const { return _port; }
        bool dns_resolved() const { return _dns_resolved; }
        bool empty() const { return _lst_ip.empty(); }
        std::vector<IpEntry> ip_lst() const { return _lst_ip; }

    protected:

    private:
        // fill entry from source infos
        static void _from_sockaddr(IpEntry *entry, const sockaddr_in *addr_in, int socktype, int protocol);
        static void _from_sockaddr(IpEntry *entry, const sockaddr_in6 *addr_in, int socktype, int protocol);
        static bool _from_addrinfo(IpEntry *entry, addrinfo *info, bool ipv6 = false);
        // adds hint to dns lookup
        static void _fill_dns_lookup_hints(struct addrinfo *hints);
        // memset to 0 struct
        inline static void _purge_ipsockaddr(IpSockAddr & ipsockaddr);

        // go through saved ips to look for socktype/protocol in ipv4/ipv6
        const IpEntry *_get_ip_info(int socktype, int protocol, bool ipv6) const;

        void _add_ip(const sockaddr_in & addr, int socktype, int protocol);
        void _add_ip(const sockaddr_in6 & addr, int socktype, int protocol);
        void _add_ip(std::string_view ip, int socktype, int protocol);
        void _add_ip(const IpEntry & ip);

        bool _netmask_from_str(std::string_view mask_value);
        void _fill_subnet();
        void _reset_subnet();
        void _dump_netdata();

        static char _ip_buffer[];

        std::string _host;
        int _port;
        bool _prefers_ipv6;

        std::string _hostname;
        std::vector<IpEntry> _lst_ip;
        bool _dns_resolved;

        bool _has_subnet;
        Subnet _subnet;
};

} // namespace sihd::net

#endif
