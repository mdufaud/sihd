#ifndef __SIHD_NET_IPADDR_HPP__
# define __SIHD_NET_IPADDR_HPP__

# include <sihd/net/Ip.hpp>

namespace sihd::net
{

// permits easy ipv4 / ipv6 manipulation
struct IpSockAddr
{
    int type = 0;
    socklen_t addr_len = 0;
    sockaddr *addr = nullptr;
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
};

class IpAddr
{
    public:
        IpAddr();
        IpAddr(int port, bool ipv6 = false);
        IpAddr(const std::string & host, bool dns_lookup = false);
        IpAddr(const std::string & host, int port, bool dns_lookup = false);
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

            std::string ip() const
            {
                return ipv6 ? IpAddr::ip_to_string(this->addr6) : IpAddr::ip_to_string(this->addr);
            }
        };

        // represents info from dns lookup
        struct DnsInfo
        {
            std::string hostname;
            std::vector<IpEntry> lst_ip;
        };

        // do a DNS lookup to find every ip addr for every socket and every protocols
        static std::optional<DnsInfo> dns_lookup(const std::string & host, bool ipv6 = false);
        static std::string fetch_ip_name(const std::string & ip);
        static std::string fetch_ip_name(sockaddr *addr, socklen_t addr_len);

        // checks both ipv4 and ipv6
        static bool is_valid_ip(const std::string & ip);
        static bool is_valid_ipv4(const std::string & ip);
        static bool is_valid_ipv6(const std::string & ip);
        static std::string ip_to_string(const sockaddr_in & addr_in);
        static std::string ip_to_string(const sockaddr_in6 & addr_in);
        static std::string ip_to_string(const in_addr & addr_in);
        static std::string ip_to_string(const in6_addr & addr_in);

        /*
            depending on ip, fills ipsockaddr addr, addr_len and type with corresponding ipv4 or ipv6 struct
            port is optionnal and setted in addr_in or addr_in6
        */
        static bool fill_sockaddr(const std::string & ip, IpSockAddr & ipsockaddr, int port = 0);
        // fills sockaddr_in from ip/port
        static bool to_sockaddr_in(sockaddr_in *filled, const std::string & ip, int port = 0);
        // fills sockaddr_in6 from ip(v6)/port
        static bool to_sockaddr_in6(sockaddr_in6 *filled, const std::string & ip, int port = 0);
        // fills sockaddr_in from ip/port
        static bool to_sockaddr_in(sockaddr_in *filled, const sockaddr & addr, size_t addr_len);
        // fills sockaddr_in6 from ip(v6)/port
        static bool to_sockaddr_in6(sockaddr_in6 *filled, const sockaddr & addr, size_t addr_len);

        // build IpAddr from localhost
        static IpAddr get_localhost(int port, bool ipv6 = false);

        // apply mask on src to dst to get netid addr
        static void get_network_id(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask);
        static void get_network_id(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask);
        // apply mask on src to dst to get broadcast addr
        static void get_broadcast(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask);
        static void get_broadcast(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask);
        // transform value to network mask
        static uint32_t value_to_mask(uint32_t value);
        // check for valid netmask
        static bool is_valid_mask(uint32_t mask);

        // remove any ip info
        void clear();

        // subnets
        bool is_same_subnet(const std::string & ip) const;
        bool is_same_subnet(const sockaddr_in & addr) const;
        bool is_same_subnet(const IpAddr & addr) const;
        
        bool set_subnet_mask(const std::string & mask);
        bool set_subnet_mask(uint32_t mask);
        bool has_subnet() const { return _has_subnet; }
        std::string dump_subnet() const;
        const Subnet & subnet() const { return _subnet; }
        size_t subnet_value() const;

        // clear and set ip from source
        void from_any(int port, bool ipv6 = false);
        bool from(const std::string & host, int port = 0);
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
        std::string get_ip(int socktype, int protocol, bool ipv6 = false) const;
        // returns string ip address for socket type
        std::string get_socktype_ip(int socktype, bool ipv6 = false) const { return this->get_ip(socktype, -1, ipv6); }
        // returns string ip address for protocol
        std::string get_protocol_ip(int protocol, bool ipv6 = false) const { return this->get_ip(-1, protocol, ipv6); }

        std::string get_first_ipv4() const { return this->get_ip(-1, -1, false); }
        std::string get_first_ipv6() const { return this->get_ip(-1, -1, true); }
        std::string get_first_ip() const { return this->prefers_ipv6() ? this->get_first_ipv6() : this->get_first_ipv4(); }

        // getters
        const std::string & host() const { return _host; }
        const std::string & hostname() const { return _hostname; }
        int port() const { return _port; }

        bool dns_resolved() const { return _dns_resolved; }

        bool empty() const { return _lst_ip.empty(); }
        size_t count_ip() const { return _lst_ip.size(); }
        const std::vector<IpEntry> lst_ip() const { return _lst_ip; }

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
        IpEntry *_get_ip_info(int socktype, int protocol, bool ipv6) const;

        void _add_ip(const sockaddr_in & addr, int socktype, int protocol);
        void _add_ip(const sockaddr_in6 & addr, int socktype, int protocol);
        void _add_ip(const std::string & ip, int socktype, int protocol);
        void _add_ip(const IpEntry & ip);

        bool _netmask_from_str(const std::string & mask_value);
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

}

#endif 