#ifndef __SIHD_PCAP_PCAPRECORDER_HPP__
# define __SIHD_PCAP_PCAPRECORDER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::pcap
{

class PcapRecorder:   public sihd::util::Named
{
    public:
        PcapRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapRecorder();

    protected:

    private:
};

}

#endif