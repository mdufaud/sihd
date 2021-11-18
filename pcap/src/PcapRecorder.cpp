#include <sihd/pcap/PcapRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(PcapRecorder)

LOGGER;

PcapRecorder::PcapRecorder(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

PcapRecorder::~PcapRecorder()
{
}

}