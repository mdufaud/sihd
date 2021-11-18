#include <sihd/pcap/PcapReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(PcapReader)

LOGGER;

PcapReader::PcapReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

PcapReader::~PcapReader()
{
}

}