#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sihd/sys/linux/dbus.hpp>

# include "internal/desktop_bus.hpp"
#endif

#include <sihd/sys/network.hpp>

namespace sihd::sys::network
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr ZStringView network_manager_service = "org.freedesktop.NetworkManager";

} // namespace

Result<Connectivity> connectivity()
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    // asking about the network must not start a network manager
    connection.disable_auto_start();
    auto owned = connection.name_has_owner(network_manager_service);
    SIHD_UNEXPECTED_RETURN(owned);
    if (!*owned)
        return std::unexpected(Error(not_supported, "no NetworkManager on this system"));

    auto state = dbus::get_property<uint32_t>(connection,
                                              network_manager_service,
                                              "/org/freedesktop/NetworkManager",
                                              network_manager_service,
                                              "Connectivity",
                                              internal::dbus_call_timeout);
    SIHD_UNEXPECTED_RETURN(state);
    switch (*state)
    {
        case 1:
            return Connectivity::none;
        case 2:
            return Connectivity::portal;
        case 3:
            return Connectivity::limited;
        case 4:
            return Connectivity::full;
        default:
            return Connectivity::unknown;
    }
}

#else

Result<Connectivity> connectivity()
{
    return std::unexpected(Error(not_supported, "network connectivity is not supported on this platform"));
}

#endif

} // namespace sihd::sys::network
