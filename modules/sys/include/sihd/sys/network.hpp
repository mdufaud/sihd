#ifndef __SIHD_SYS_NETWORK_HPP__
#define __SIHD_SYS_NETWORK_HPP__

#include <sihd/sys/desktop.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::network
{

constexpr bool supported = desktop::supported;

enum class Connectivity
{
    unknown,
    none,
    // behind a captive portal, linux only
    portal,
    // a network without the internet
    limited,
    full
};

// as the system network manager sees it, not_supported on a linux without NetworkManager
sihd::util::Result<Connectivity> connectivity();

} // namespace sihd::sys::network

#endif
