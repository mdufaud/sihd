#include <sihd/pcap/PcapUtils.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::pcap
{

SIHD_NEW_LOGGER("sihd::pcap");

bool PcapUtils::_is_init = false;

std::string PcapUtils::status_str(int code)
{
    return pcap_statustostr(code);
}

std::string PcapUtils::version()
{
    return pcap_lib_version();
}

bool    PcapUtils::init(unsigned int opts)
{
    if (PcapUtils::_is_init)
        return true;
    char errbuf[PCAP_ERRBUF_SIZE];
    PcapUtils::_is_init = pcap_init(opts, errbuf) == 0;
    if (PcapUtils::_is_init == false)
        SIHD_LOG(error, "PcapUtils: " << errbuf);
    return PcapUtils::_is_init;
}

bool    PcapUtils::lookupnet(std::string_view dev, bpf_u_int32 *ip, bpf_u_int32 *mask)
{
    char errbuf[PCAP_ERRBUF_SIZE];

    int ret = pcap_lookupnet(dev.data(), ip, mask, errbuf);
    if (ret != 0)
        SIHD_LOG(error, "PcapUtils: " << errbuf);
    return ret;
}

bool    PcapUtils::is_datalink(int dtl)
{
    return pcap_datalink_val_to_name(dtl) != nullptr;
}

std::string PcapUtils::datalink_str(int dtl)
{
    return pcap_datalink_val_to_name(dtl);
}

std::string PcapUtils::datalink_desc(int dtl)
{
    return pcap_datalink_val_to_description(dtl);
}

int     PcapUtils::datalink(std::string_view dtl)
{
    return pcap_datalink_name_to_val(dtl.data());
}

bool    PcapUtils::is_timestamp_type(int ts)
{
    return pcap_tstamp_type_val_to_name(ts) != nullptr;
}

std::string PcapUtils::timestamp_type_str(int ts)
{
    return pcap_tstamp_type_val_to_name(ts);
}

std::string PcapUtils::timestamp_type_desc(int ts)
{
    return pcap_tstamp_type_val_to_description(ts);
}

int     PcapUtils::timestamp_type(std::string_view ts)
{
    return pcap_tstamp_type_name_to_val(ts.data());
}

}