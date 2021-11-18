#ifndef __SIHD_PCAP_PCAPWRITER_HPP__
# define __SIHD_PCAP_PCAPWRITER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::pcap
{

class PcapWriter:   public sihd::util::Named
{
    public:
        PcapWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapWriter();

    protected:

    private:
};

}

#endif