#ifndef __SIHD_PCAP_PCAPUTILS_HPP__
#define __SIHD_PCAP_PCAPUTILS_HPP__

#include <cstdint>
#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::pcap::utils
{

bool init(int opts = -1);

sihd::util::Result<void> lookupnet(sihd::util::ZStringView dev, uint32_t *ip, uint32_t *mask);

std::string status_str(int code);
std::string version();

bool is_timestamp_type(int ts);
std::string timestamp_type_str(int ts);
std::string timestamp_type_desc(int dtl);
int timestamp_type(sihd::util::ZStringView dtl);

bool is_datalink(int dtl);
std::string datalink_str(int dtl);
std::string datalink_desc(int dtl);
int datalink(sihd::util::ZStringView dtl);

} // namespace sihd::pcap::utils

#endif