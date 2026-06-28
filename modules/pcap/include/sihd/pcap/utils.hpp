#ifndef __SIHD_PCAP_PCAPUTILS_HPP__
#define __SIHD_PCAP_PCAPUTILS_HPP__

#include <cstdint>
#include <string>
#include <string_view>

namespace sihd::pcap::utils
{

bool init(int opts = -1);

bool lookupnet(std::string_view dev, uint32_t *ip, uint32_t *mask);

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