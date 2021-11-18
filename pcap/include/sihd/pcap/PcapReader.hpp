#ifndef __SIHD_PCAP_PCAPREADER_HPP__
# define __SIHD_PCAP_PCAPREADER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::pcap
{

class PcapReader:   public sihd::util::Named
{
    public:
        PcapReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapReader();

    protected:

    private:
};

}

#endif