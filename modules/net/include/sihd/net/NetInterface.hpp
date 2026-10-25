#ifndef __SIHD_NET_NETINTERFACE_HPP__
#define __SIHD_NET_NETINTERFACE_HPP__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class NetInterface
{
    public:
        enum class Kind
        {
            unknown,
            loopback,
            ethernet,
            wifi,
            cellular,
            tunnel,
            ppp,
            bridge,
        };

        struct Address
        {
                IpAddr addr;
                IpAddr netmask;
                IpAddr extra_addr;
        };

        NetInterface();
        ~NetInterface();

        static sihd::util::Result<std::map<std::string, NetInterface>> get_all_interfaces();
        static std::string_view kind_str(Kind kind);

        const std::string & name() const;
        const std::string & mac_addr() const;
        uint32_t index() const;
        Kind kind() const;
        // false for loopback and virtual links: tunnels, bridges, veth, TAP
        bool hardware() const;
        uint32_t mtu() const;
        // 0 when unknown
        uint64_t link_speed_bps() const;
        const std::vector<Address> & addresses() const;

        const IpAddr & addr4() const;
        const IpAddr & netmask4() const;
        // either point-to-point link or broadcast
        const IpAddr & extra_addr4() const;

        const IpAddr & addr6() const;
        const IpAddr & netmask6() const;
        // either point-to-point link or broadcast
        const IpAddr & extra_addr6() const;

        bool up() const;
        bool broadcast() const;
        bool loopback() const;
        bool point2point() const;
        bool running() const;
        bool noarp() const;
        bool promisc() const;
        bool notrailers() const;
        bool master() const;
        bool slave() const;
        bool all_multicast() const;
        bool supports_multicast() const;

    protected:
        void set_name(std::string_view name);
        void set_flags(uint32_t flags);
        void set_macaddr(const std::string & mac_addr);
        void set_index(uint32_t index);
        void set_kind(Kind kind);
        void set_hardware(bool hardware);
        void set_mtu(uint32_t mtu);
        void set_link_speed_bps(uint64_t link_speed_bps);
        // addr4() and addr6() report the last address added of their family
        void add_address(Address && address);
        // listed by addresses() only
        void add_alias_address(Address && address);

    private:
        const Address & reported_address(const std::optional<size_t> & index) const;

        std::string _name;
        uint32_t _flags = 0;
        uint32_t _index = 0;
        Kind _kind = Kind::unknown;
        bool _hardware = false;
        uint32_t _mtu = 0;
        uint64_t _link_speed_bps = 0;
        std::vector<Address> _addresses;
        std::optional<size_t> _ipv4_index;
        std::optional<size_t> _ipv6_index;

        std::string _mac_address;
};

} // namespace sihd::net

#endif
