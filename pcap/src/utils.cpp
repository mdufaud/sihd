#include <mutex>

#include <sihd/util/Logger.hpp>

#include <sihd/pcap/utils.hpp>

namespace sihd::pcap::utils
{

SIHD_NEW_LOGGER("sihd::pcap::utils");

namespace
{
std::mutex _init_mutex;
bool _is_init = false;
} // namespace

std::string status_str(int code)
{
    return pcap_statustostr(code);
}

std::string version()
{
    return pcap_lib_version();
}

bool init(int opts)
{
    std::lock_guard l(_init_mutex);
    if (_is_init)
        return true;
#if defined(PCAP_CHAR_ENC_UTF_8)
    if (opts < 0)
        opts = PCAP_CHAR_ENC_UTF_8;
    char errbuf[PCAP_ERRBUF_SIZE];
    _is_init = pcap_init(opts, errbuf) == 0;
    if (_is_init == false)
        SIHD_LOG(error, "{}", errbuf);
#else
    (void)opts;
# if defined(__SIHD_WINDOWS__)
    _is_init = pcap_wsockinit() == 0;
# else
    _is_init = true;
# endif
#endif
    return _is_init;
}

bool lookupnet(std::string_view dev, bpf_u_int32 *ip, bpf_u_int32 *mask)
{
    char errbuf[PCAP_ERRBUF_SIZE];

    int ret = pcap_lookupnet(dev.data(), ip, mask, errbuf);
    if (ret != 0)
        SIHD_LOG(error, "{}", errbuf);
    return ret == 0;
}

bool is_datalink(int dtl)
{
    return pcap_datalink_val_to_name(dtl) != nullptr;
}

std::string datalink_str(int dtl)
{
    return pcap_datalink_val_to_name(dtl);
}

std::string datalink_desc(int dtl)
{
    return pcap_datalink_val_to_description(dtl);
}

int datalink(std::string_view dtl)
{
    return pcap_datalink_name_to_val(dtl.data());
}

bool is_timestamp_type(int ts)
{
    return pcap_tstamp_type_val_to_name(ts) != nullptr;
}

std::string timestamp_type_str(int ts)
{
    return pcap_tstamp_type_val_to_name(ts);
}

std::string timestamp_type_desc(int ts)
{
    return pcap_tstamp_type_val_to_description(ts);
}

int timestamp_type(std::string_view ts)
{
    return pcap_tstamp_type_name_to_val(ts.data());
}

} // namespace sihd::pcap::utils
