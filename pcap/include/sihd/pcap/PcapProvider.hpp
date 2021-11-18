#ifndef __SIHD_PCAP_PCAPPROVIDER_HPP__
# define __SIHD_PCAP_PCAPPROVIDER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::pcap
{

class PcapProvider:   public sihd::util::Named
{
    public:
        PcapProvider(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapProvider();

    protected:

    private:
};

}

#endif