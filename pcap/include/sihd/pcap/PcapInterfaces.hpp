#ifndef __SIHD_PCAP_PCAPINTERFACES_HPP__
# define __SIHD_PCAP_PCAPINTERFACES_HPP__

# include <sihd/pcap/PcapUtils.hpp>
# include <string>
# include <vector>

namespace sihd::pcap
{

class PcapIFace
{

    public:
        PcapIFace(pcap_if_t *ptr);
        ~PcapIFace();

        std::string name() const { return _if_ptr->name; };
        std::string description() const { return _if_ptr->description; };

        bool loopback() const { return _if_ptr->flags & PCAP_IF_LOOPBACK; }
        bool up() const { return _if_ptr->flags & PCAP_IF_UP; }
        bool running() const { return _if_ptr->flags & PCAP_IF_RUNNING; }
        bool wireless() const { return _if_ptr->flags & PCAP_IF_WIRELESS; }
        bool connection_unknown() const { return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_UNKNOWN; }
        bool connected() const { return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_CONNECTED; }
        bool disconnected() const { return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED; }
        bool can_be_connected() const { return !(_if_ptr->flags & PCAP_IF_CONNECTION_STATUS_NOT_APPLICABLE); }

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

        struct pcap_addr *get_addr(std::string_view name);
        pcap_if_t *get() { return _interfaces_ptr; }

    protected:

    private:
        void _free();

        int _code;
        pcap_if_t *_interfaces_ptr;

};

}

#endif