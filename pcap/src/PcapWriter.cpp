#include <sihd/pcap/PcapWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(PcapWriter)

LOGGER;

PcapWriter::PcapWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

PcapWriter::~PcapWriter()
{
}

}