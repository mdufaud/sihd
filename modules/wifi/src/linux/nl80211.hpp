#ifndef __SIHD_WIFI_LINUX_NL80211_HPP__
#define __SIHD_WIFI_LINUX_NL80211_HPP__

#include <cstdint>
#include <string>

#include <sihd/net/linux/netlink.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::wifi::nl80211
{

// opens a generic netlink channel: not_supported unless a cfg80211 wifi stack runs
sihd::util::Result<uint16_t> open(sihd::net::linux_::netlink::Netlink & channel);

sihd::net::linux_::netlink::Request request(uint16_t family, uint8_t command, uint16_t flags);

sihd::util::Result<sihd::net::linux_::netlink::Message>
    find_interface(sihd::net::linux_::netlink::Netlink & channel, uint16_t family, const std::string & name);
uint32_t ifindex(const sihd::net::linux_::netlink::Message & interface);

} // namespace sihd::wifi::nl80211

#endif
