#include <fcntl.h>    // fcntl
#include <sys/time.h> // timeval

#include <cstring>
#include <expected>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

#include <net/if.h> // IFNAMSIZ

namespace sihd::net
{

using sihd::util::Error;
using sihd::util::ErrorCode;

SIHD_LOGGER;

bool Socket::get_socket_infos(int socket, int *domain, int *type, int *protocol)
{
    socklen_t length = sizeof(int);
    bool found = sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_DOMAIN, domain, &length).has_value();
    length = sizeof(int);
    found = found && sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_TYPE, type, &length).has_value();
    length = sizeof(int);
    found = found && sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_PROTOCOL, protocol, &length).has_value();
    return found;
}

sihd::util::Result<void> Socket::bind_socket_to_device(int socket, std::string_view name)
{
    if (name.empty())
        return std::unexpected(Error(ErrorCode::invalid_argument, "empty device name"));
    if (name.size() >= IFNAMSIZ)
        return std::unexpected(Error(ErrorCode::invalid_argument, "device name too long: {}", name));
    char device_name[IFNAMSIZ] = {0};
    memcpy(device_name, name.data(), name.size());
    auto res = sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, device_name, sizeof(device_name));
    SIHD_UNEXPECTED_RETURN_CTX(res, "could not bind to device");
    return {};
}

sihd::util::Result<void> Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::closed, "cannot set blocking on a closed socket"));
    int opts = ::fcntl(socket, F_GETFL);
    if (opts < 0)
        return std::unexpected(make_error("could not get fcntl"));
    if (active)
        opts &= ~O_NONBLOCK;
    else
        opts |= O_NONBLOCK;
    if (::fcntl(socket, F_SETFL, opts) < 0)
        return std::unexpected(make_error("could not set fcntl options"));
    return {};
}

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        return false;
    int opts = ::fcntl(socket, F_GETFL);
    // a predicate has no error channel: the log is the only trace
    if (opts < 0)
    {
        SIHD_LOG(error, "Socket: could not get fcntl: {}", sihd::sys::os::last_error_str());
        return false;
    }
    return !(opts & O_NONBLOCK);
}

sihd::util::Result<void> Socket::set_socket_recv_timeout(int socket, int milliseconds)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::closed, "cannot set recv timeout on a closed socket"));
    const timeval tv {milliseconds / 1000, (milliseconds % 1000) * 1000};
    auto res = sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    SIHD_UNEXPECTED_RETURN_CTX(res, "could not set recv timeout");
    return {};
}

} // namespace sihd::net
