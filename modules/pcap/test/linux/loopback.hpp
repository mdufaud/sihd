#ifndef __SIHD_PCAP_TEST_LOOPBACK_HPP__
#define __SIHD_PCAP_TEST_LOOPBACK_HPP__

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <string_view>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Result.hpp>

#include <net/if.h>

namespace test
{

inline sihd::util::Result<void> set_loopback_up()
{
    sihd::net::Socket socket;
    if (auto opened = socket.open(AF_INET, SOCK_DGRAM, 0); !opened)
        return opened;
    struct ifreq ifr {};
    std::string_view("lo").copy(ifr.ifr_name, IFNAMSIZ - 1);
    if (auto flags = sihd::sys::os::ioctl(socket.socket(), SIOCGIFFLAGS, &ifr); !flags)
        return flags;
    ifr.ifr_flags = static_cast<short>(ifr.ifr_flags | IFF_UP);
    return sihd::sys::os::ioctl(socket.socket(), SIOCSIFFLAGS, &ifr);
}

} // namespace test

#endif
