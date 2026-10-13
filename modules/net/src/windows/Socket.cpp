#include <ws2tcpip.h> // CSADDR_INFO

#include <expected>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// missing mingw getsockopt action
#ifndef SO_BSP_STATE
# define SO_BSP_STATE 0x1009
#endif

namespace sihd::net
{

using sihd::util::Error;
using sihd::util::ErrorCode;

SIHD_LOGGER;

bool Socket::get_socket_infos(int socket, int *domain, int *type, int *protocol)
{
    // SO_BSP_STATE returns a CSADDR_INFO whose LocalAddr/RemoteAddr point into the same buffer right
    // after the struct: it must fit both appended sockaddrs or getsockopt fails with WSAEFAULT
    char buffer[sizeof(CSADDR_INFO) + 2 * sizeof(SOCKADDR_STORAGE)];
    CSADDR_INFO *addrinfo = reinterpret_cast<CSADDR_INFO *>(buffer);
    socklen_t length = sizeof(buffer);
    auto found = sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_BSP_STATE, addrinfo, &length);
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
    return found.has_value();
}

std::expected<void, Error> Socket::bind_socket_to_device(int socket, std::string_view name)
{
    (void)socket;
    (void)name;
    return std::unexpected(Error(ErrorCode::not_supported, "bind to device unsupported on windows"));
}

std::expected<void, Error> Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::closed, "cannot set blocking on a closed socket"));
    unsigned long mode = active ? 0 : 1;
    auto res = sihd::sys::os::ioctl(socket, FIONBIO, &mode);
    SIHD_UNEXPECTED_RETURN_CTX(res, "could not set blocking mode");
    return {};
}

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        return false;
    // winsock provides no way to read the mode back: the default is reported
    return true;
}

std::expected<void, Error> Socket::set_socket_recv_timeout(int socket, int milliseconds)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::closed, "cannot set recv timeout on a closed socket"));
    DWORD ms = static_cast<DWORD>(milliseconds);
    auto res = sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &ms, sizeof(ms));
    SIHD_UNEXPECTED_RETURN_CTX(res, "could not set recv timeout");
    return {};
}

} // namespace sihd::net
