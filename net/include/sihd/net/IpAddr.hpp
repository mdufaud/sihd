#ifndef __SIHD_NET_IPADDR_HPP__
# define __SIHD_NET_IPADDR_HPP__

# include <sihd/net/Ip.hpp>

namespace sihd::net
{

class IpAddr
{
    public:
        IpAddr(const std::string & host, int port = 0, bool dns_lookup = true);
        IpAddr(const sockaddr_in & addr, bool dns_lookup = false);
        IpAddr(const sockaddr_in6 & addr, bool dns_lookup = false);

        virtual ~IpAddr();

        struct IpInfo
        {
            bool ipv6;
            int socktype;
            int protocol;
            std::string ip;
            in_addr addr;
            in6_addr addr6;
        };

        struct DnsInfo
        {
            bool resolved = false;
            std::string hostname;
            std::vector<IpInfo> lst_ip;
        };

        // do a DNS lookup to find every ip addr for every socket and every protocols
        static std::optional<DnsInfo> dns_lookup(const std::string & host, bool ipv6 = false);
        static std::string fetch_ip_name(const std::string & ip);
        static std::string fetch_ip_name(sockaddr *addr, socklen_t addr_len);

        static bool is_valid_ip(const std::string & ip);
        static bool is_valid_ipv4(const std::string & ip);
        static bool is_valid_ipv6(const std::string & ip);
        static std::string ip_to_string(const sockaddr_in & addr_in);
        static std::string ip_to_string(const sockaddr_in6 & addr_in);
        /*
            depending on ip:
                - fills addr_in or addr_in6
                - fill addr with addr_in or addr_in6
                - addrlen with sizeof of addr_in or addr_in6
                - type to AF_INET or AF_INET6
            port is optionnal and setted in addr_in or addr_in6
        */
        static bool fill_sockaddr(const std::string & ip, sockaddr **addr, socklen_t *addrlen, int *type,
                                    sockaddr_in *addr_in, sockaddr_in6 *addr_in6, int port = 0);
        static std::optional<sockaddr_in> to_sockaddr_in(const std::string & ip, int port = 0);
        static std::optional<sockaddr_in6> to_sockaddr_in6(const std::string & ip, int port = 0);

        // do a dns_lookup and fill internal object
        bool do_lookup_dns(bool ipv6 = false);

        // returns ip address taken from DNS
        std::optional<sockaddr_in> get_sockaddr_in(int socktype, int protocol) const;
        std::optional<sockaddr_in> get_first_sockaddr_in() const
            { return this->get_sockaddr_in(-1, -1); }
        std::optional<sockaddr_in6> get_sockaddr_in6(int socktype, int protocol) const;
        std::optional<sockaddr_in6> get_first_sockaddr_in6() const
            { return this->get_sockaddr_in6(-1, -1); }

        // returns string ip address taken from DNS
        std::string get_ip(int socktype, int protocol, bool ipv6 = false) const;
        // returns string ip address taken from DNS for socket type
        std::string get_socktype_ip(int socktype, bool ipv6 = false) const { return this->get_ip(socktype, -1, ipv6); }
        // returns string ip address taken from DNS for protocol
        std::string get_protocol_ip(int protocol, bool ipv6 = false) const { return this->get_ip(-1, protocol, ipv6); }
        std::string get_first_ipv4() const { return this->get_ip(-1, -1, false); }
        std::string get_first_ipv6() const { return this->get_ip(-1, -1, true); }

        const std::string & hostname() const { return _dns.hostname; }
        const std::string & host() const { return _host; }
        int port() const { return _port; }

        const DnsInfo & dns() const { return _dns; }

        bool dns_resolved() const { return _dns.resolved; }

    protected:
    
    private:
        static std::optional<IpInfo> _from_addrinfo(addrinfo *info, bool ipv6 = false);
        static IpInfo _from_sockaddr(const sockaddr_in *addr_in, int socktype, int protocol);
        static IpInfo _from_sockaddr(const sockaddr_in6 *addr_in, int socktype, int protocol);
        static void _fill_dns_lookup_hints(struct addrinfo *hints);

        IpInfo *_get_ip_info(int socktype, int protocol, bool ipv6) const;
        void _add_ip(const sockaddr_in & addr, int socktype, int protocol);
        void _add_ip(const sockaddr_in6 & addr, int socktype, int protocol);
        void _add_ip(const std::string & ip, int socktype, int protocol);

        static char _ip_buffer[];

        std::string _host;
        int _port;
        DnsInfo _dns;
};

}

#endif 