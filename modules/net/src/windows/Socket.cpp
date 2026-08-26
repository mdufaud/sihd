#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include <ws2tcpip.h> // CSADDR_INFO

// missing mingw getsockopt action
# ifndef SO_BSP_STATE
#  define SO_BSP_STATE 0x1009
# endif

namespace sihd::net
{

using namespace sihd::util;

SIHD_LOGGER;

bool Socket::get_socket_infos(int socket, int *domain, int *type, int *protocol)
{
    // SO_BSP_STATE returns a CSADDR_INFO whose LocalAddr/RemoteAddr point into the
    // same buffer right after the struct; the buffer must be large enough for both
    // appended sockaddrs or getsockopt fails with WSAEFAULT.
    char buffer[sizeof(CSADDR_INFO) + 2 * sizeof(SOCKADDR_STORAGE)];
    CSADDR_INFO *addrinfo = reinterpret_cast<CSADDR_INFO *>(buffer);
    socklen_t length = sizeof(buffer);
    bool found = sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_BSP_STATE, addrinfo, &length);
    if (found)
    {
        *protocol = addrinfo->iProtocol;
        *type = addrinfo->iSocketType;
        if (addrinfo->LocalAddr.lpSockaddr != nullptr)
            *domain = addrinfo->LocalAddr.lpSockaddr->sa_family;
        else if (addrinfo->RemoteAddr.lpSockaddr != nullptr)
            *domain = addrinfo->RemoteAddr.lpSockaddr->sa_family;
        else
            *domain = AF_INET;
    }
    return found;
}

bool Socket::bind_socket_to_device(int socket, std::string_view name)
{
    (void)socket;
    (void)name;
    return false;
}

bool Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set blocking on a closed socket");
    unsigned long mode = active ? 0 : 1;
    if (!sihd::sys::os::ioctl(socket, FIONBIO, &mode))
    {
        SIHD_LOG(error, "Socket: could not set ioctl: {}", sihd::sys::os::last_error_str());
        return false;
    }
    return true;
}

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        throw std::runtime_error("Socket: check blocking on a closed socket");
    /// @note windows sockets are created in blocking mode by default
    // currently on windows, there is no easy way to obtain the socket's current blocking mode since
    // WSAIsBlocking was deprecated
    unsigned long mode = 1;
    bool set_blocking = sihd::sys::os::ioctl(socket, FIONBIO, &mode);
    if (set_blocking)
    {
        // put back non blocking
        mode = 0;
        return sihd::sys::os::ioctl(socket, FIONBIO, &mode, true);
    }
    return set_blocking == false;
}

} // namespace sihd::net
