#ifndef __SIHD_PCAP_PCAPINTERFACES_HPP__
#define __SIHD_PCAP_PCAPINTERFACES_HPP__

#include <string>
#include <vector>

#include <sihd/pcap/utils.hpp>

namespace sihd::pcap
{

class PcapIFace
{
    public:
        PcapIFace(pcap_if_t *ptr);
        ~PcapIFace();

        std::string name() const;
        std::string description() const;

        bool loopback() const;
        bool up() const;
        bool running() const;
        bool wireless() const;
        bool connection_unknown() const;
        bool connected() const;
        bool disconnected() const;
        bool can_be_connected() const;

        const std::vector<struct pcap_addr *> & addresses() const;
        std::string dump() const;

    private:
        pcap_if_t *_if_ptr;
        std::vector<struct pcap_addr *> _addr;
};

class PcapInterfaces
{
    public:
        PcapInterfaces();
        virtual ~PcapInterfaces();
        bool find();
        bool error();

        std::vector<std::string> names();
        std::vector<PcapIFace> ifaces();

        std::string status();

        struct pcap_addr *pcap_addr(std::string_view name);
        pcap_if_t *get() { return _interfaces_ptr; }

    protected:

    private:
        void _free();

        int _code;
        pcap_if_t *_interfaces_ptr;
};

} // namespace sihd::pcap

#endif