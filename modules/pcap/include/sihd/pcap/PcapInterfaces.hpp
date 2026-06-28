#ifndef __SIHD_PCAP_PCAPINTERFACES_HPP__
#define __SIHD_PCAP_PCAPINTERFACES_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>

namespace sihd::pcap
{

struct PcapAddress
{
        sihd::net::IpAddr addr;
        sihd::net::IpAddr netmask;
        sihd::net::IpAddr broadaddr;
        sihd::net::IpAddr dstaddr;
};

class PcapIFace
{
    public:
        PcapIFace(std::string name, std::string description, uint32_t flags, std::vector<PcapAddress> addresses);
        ~PcapIFace();

        const std::string & name() const;
        const std::string & description() const;

        bool loopback() const;
        bool up() const;
        bool running() const;
        bool wireless() const;
        bool connection_unknown() const;
        bool connected() const;
        bool disconnected() const;
        bool can_be_connected() const;

        const std::vector<PcapAddress> & addresses() const;
        std::string dump() const;

    private:
        std::string _name;
        std::string _description;
        uint32_t _flags;
        std::vector<PcapAddress> _addresses;
};

class PcapInterfaces
{
    public:
        PcapInterfaces();
        ~PcapInterfaces();
        bool find();
        bool error();

        std::vector<std::string> names();
        std::vector<PcapIFace> ifaces();

        std::string status();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::pcap

#endif