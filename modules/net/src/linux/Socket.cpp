#include <algorithm>

#include <fcntl.h> // fcntl
#include <net/if.h> // IFNAMSIZ

#include <cstring>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

using namespace sihd::util;

SIHD_LOGGER;

bool Socket::get_socket_infos(int socket, int *domain, int *type, int *protocol)
{
    socklen_t length = sizeof(int);
    bool found = sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_DOMAIN, domain, &length);
    length = sizeof(int);
    found = found && sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_TYPE, type, &length);
    length = sizeof(int);
    found = found && sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_PROTOCOL, protocol, &length);
    return found;
}

bool Socket::bind_socket_to_device(int socket, std::string_view name)
{
    char device_name[IFNAMSIZ];

    strncpy(device_name, name.data(), std::min(name.size(), (size_t)IFNAMSIZ));
    return sihd::sys::os::setsockopt(socket,
                                     SOL_SOCKET,
                                     SO_BINDTODEVICE,
                                     device_name,
                                     sizeof(device_name),
                                     true);
}

bool Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set blocking on a closed socket");
    int opts = ::fcntl(socket, F_GETFL);
    if (opts < 0)
    {
        SIHD_LOG(error, "Socket: could not get fcntl: {}", sihd::sys::os::last_error_str());
        return false;
    }
    if (active)
        opts &= ~O_NONBLOCK;
    else
        opts |= O_NONBLOCK;
    opts = ::fcntl(socket, F_SETFL, opts);
    if (opts < 0)
        SIHD_LOG(error, "Socket: could not set fcntl options: {}", sihd::sys::os::last_error_str());
    return opts >= 0;
}

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot check blocking on a closed socket");
    int opts = ::fcntl(socket, F_GETFL);
    if (opts < 0)
    {
        SIHD_LOG(error, "Socket: could not get fcntl: {}", sihd::sys::os::last_error_str());
        return false;
    }
    return !(opts & O_NONBLOCK);
}

} // namespace sihd::net
