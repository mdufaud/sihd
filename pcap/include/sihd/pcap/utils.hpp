#ifndef __SIHD_PCAP_PCAPUTILS_HPP__
#define __SIHD_PCAP_PCAPUTILS_HPP__

#include <string>
#include <vector>

#pragma message("TODO forward")
#include <pcap.h>

#include <sihd/util/platform.hpp>

namespace sihd::pcap::utils
{

bool init(int opts = -1);

// TODO return net::IpAddr
bool lookupnet(std::string_view dev, bpf_u_int32 *ip, bpf_u_int32 *mask);

std::string status_str(int code);
std::string version();

bool is_timestamp_type(int ts);
std::string timestamp_type_str(int ts);
std::string timestamp_type_desc(int dtl);
int timestamp_type(std::string_view dtl);

bool is_datalink(int dtl);
std::string datalink_str(int dtl);
std::string datalink_desc(int dtl);
int datalink(std::string_view dtl);

} // namespace sihd::pcap::utils

#endif