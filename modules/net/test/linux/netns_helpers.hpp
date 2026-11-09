#ifndef __SIHD_NET_TEST_NETNS_HELPERS_HPP__
#define __SIHD_NET_TEST_NETNS_HELPERS_HPP__

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <array>
#include <cstring>
#include <map>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Result.hpp>

#include <linux/ipv6.h>
#include <net/if.h>

namespace test
{

inline constexpr int child_ok = 0;
inline constexpr int child_failed = 1;
inline constexpr int child_tun_unavailable = 3;
inline constexpr std::array<sihd::sys::linux_::ns::Type, 1> network_namespace = {sihd::sys::linux_::ns::Type::net};

inline const std::map<int, std::string> tun_skipped_child_codes = {
    {child_tun_unavailable, "/dev/net/tun unavailable"},
};

// the forked child records its gtest failures in its own copy of the test state
inline int child_result()
{
    return ::testing::Test::HasFailure() ? child_failed : child_ok;
}

// a missing or refused /dev/net/tun skips, any other failure is a bug
inline int tun_failure(const sihd::util::Error & error)
{
    if (error.code == sihd::util::ErrorCode::not_found || error.code == sihd::util::ErrorCode::permission_denied)
        return child_tun_unavailable;
    ADD_FAILURE() << "tun creation failed: " << error.message;
    return child_failed;
}

inline ifreq make_ifreq(const std::string & name)
{
    ifreq request {};
    name.copy(request.ifr_name, IFNAMSIZ - 1);
    return request;
}

inline sihd::util::Result<sihd::net::Socket> open_control()
{
    sihd::net::Socket control;
    auto opened = control.open(AF_INET, SOCK_DGRAM, 0);
    SIHD_UNEXPECTED_RETURN(opened);
    return control;
}

inline sihd::util::Result<void> set_up(const sihd::net::Socket & control, const std::string & name, bool up)
{
    ifreq request = make_ifreq(name);
    auto flags = sihd::sys::os::ioctl(control.socket(), SIOCGIFFLAGS, &request);
    SIHD_UNEXPECTED_RETURN(flags);
    request.ifr_flags = static_cast<short>(up ? request.ifr_flags | IFF_UP : request.ifr_flags & ~IFF_UP);
    return sihd::sys::os::ioctl(control.socket(), SIOCSIFFLAGS, &request);
}

// a /24, a label like "tun0:1" adds a secondary address
inline sihd::util::Result<void>
    set_ipv4(const sihd::net::Socket & control, const std::string & label, const std::string & address)
{
    ifreq request = make_ifreq(label);
    const sockaddr_in ip = sihd::net::IpAddr(address).addr4();
    std::memcpy(&request.ifr_addr, &ip, sizeof(ip));
    auto assigned = sihd::sys::os::ioctl(control.socket(), SIOCSIFADDR, &request);
    SIHD_UNEXPECTED_RETURN(assigned);
    if (address == "0.0.0.0")
        return {};

    const sockaddr_in netmask = sihd::net::IpAddr("255.255.255.0").addr4();
    std::memcpy(&request.ifr_netmask, &netmask, sizeof(netmask));
    return sihd::sys::os::ioctl(control.socket(), SIOCSIFNETMASK, &request);
}

// a /64: the kernel takes ipv6 addresses by interface index through an AF_INET6 socket
inline sihd::util::Result<void> set_ipv6(const std::string & name, const std::string & address)
{
    sihd::net::Socket control;
    auto opened = control.open(AF_INET6, SOCK_DGRAM, 0);
    SIHD_UNEXPECTED_RETURN(opened);
    ifreq index_request = make_ifreq(name);
    auto indexed = sihd::sys::os::ioctl(control.socket(), SIOCGIFINDEX, &index_request);
    SIHD_UNEXPECTED_RETURN(indexed);

    in6_ifreq request {};
    request.ifr6_addr = sihd::net::IpAddr(address).addr6().sin6_addr;
    request.ifr6_prefixlen = 64;
    request.ifr6_ifindex = index_request.ifr_ifindex;
    return sihd::sys::os::ioctl(control.socket(), SIOCSIFADDR, &request);
}

inline sihd::util::Result<void> configure_tun(const std::string & name, const std::string & address)
{
    auto control = open_control();
    SIHD_UNEXPECTED_RETURN(control);
    if (!sihd::sys::fs::write(fmt::format("/proc/sys/net/ipv6/conf/{}/disable_ipv6", name), "1"))
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::io_error, "could not disable ipv6 on {}", name));
    auto addressed = set_ipv4(*control, name, address);
    SIHD_UNEXPECTED_RETURN(addressed);
    return set_up(*control, name, true);
}

} // namespace test

#endif
