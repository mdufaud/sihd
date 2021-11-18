#include <sihd/pcap/PcapProvider.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(PcapProvider)

LOGGER;

PcapProvider::PcapProvider(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

PcapProvider::~PcapProvider()
{
}

}