#include <unistd.h>

#include <cstring>
#include <expected>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/tcp.h> // tcp nodelay
# include <sys/un.h>      // unix sockets

# include <net/if.h> // if_nametoindex
#else
# include <afunix.h>   // AF_UNIX
# include <ws2tcpip.h> // IP_TTL

#endif

namespace sihd::net
{

SIHD_LOGGER;

namespace
{

using sihd::util::Error;
using sihd::util::ErrorCode;

void adapt_array_size(sihd::util::IArray & arr, size_t sent)
{
    if (sent <= arr.byte_capacity() && sent != arr.byte_size())
        arr.byte_resize(sent);
}

bool interrupted_error([[maybe_unused]] int err)
{
    // winsock operations are not interrupted by signals
#if !defined(__SIHD_WINDOWS__)
    return err == EINTR;
#else
    return false;
#endif
}

template <typename Syscall>
auto retry_interrupted(Syscall && syscall)
{
    auto ret = syscall();
    while (ret < 0 && interrupted_error(sihd::sys::os::last_socket_error()))
        ret = syscall();
    return ret;
}

std::expected<ssize_t, Error> socket_call(std::string_view op, auto && syscall)
{
    auto ret = retry_interrupted(syscall);
    if (ret >= 0)
        return ret;
    const int err = sihd::sys::os::last_socket_error();
    return std::unexpected(
        Error(sihd::util::error_errno(err), "Socket: {} error: {}", op, sihd::sys::os::error_str(err)));
}

std::expected<size_t, Error> size_result(std::expected<ssize_t, Error> && res)
{
    if (res)
        return (size_t)*res;
    return std::unexpected(std::move(res).error());
}

std::expected<void, Error> opt_result(std::string_view op, bool ok)
{
    if (ok)
        return {};
    const int err = sihd::sys::os::last_socket_error();
    return std::unexpected(
        Error(sihd::util::error_errno(err), "Socket: {} error: {}", op, sihd::sys::os::error_str(err)));
}

Error closed_socket_error(std::string_view op)
{
    return Error(ErrorCode::closed, "Socket: cannot {} on a closed socket", op);
}

bool atomic_datagram_type(int type)
{
    return type == SOCK_DGRAM
#if defined(SOCK_SEQPACKET)
           || type == SOCK_SEQPACKET
#endif
#if defined(SOCK_RDM)
           || type == SOCK_RDM
#endif
        ;
}

std::optional<socklen_t> unix_path_to_sockaddr(sockaddr_un *addr, std::string_view path)
{
    // addr_len separates pathname (terminator included) from abstract (leading null kept)
    const size_t terminator = (path[0] != '\0');
    if (path.empty() || path.size() + terminator > sizeof(addr->sun_path))
        return std::nullopt;
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    memcpy(addr->sun_path, path.data(), path.size());
    return offsetof(sockaddr_un, sun_path) + path.size() + terminator;
}

std::string unix_path_from_addr(const sockaddr_un & addr, socklen_t addr_len)
{
    // an abstract name starts with the null byte a C string would stop at
    if (addr_len > (socklen_t)offsetof(sockaddr_un, sun_path) && addr.sun_path[0] == '\0')
        return std::string(addr.sun_path, addr_len - offsetof(sockaddr_un, sun_path));
    return std::string(addr.sun_path);
}

} // namespace

Socket::Socket()
{
    this->_clear_socket_info();
    _rcv_flags = 0;
    _send_flags = 0;
    _verbose = false;
}

Socket::Socket(int domain, int socket_type, int protocol): Socket()
{
    SIHD_UNEXPECTED_LOG(this->open(domain, socket_type, protocol));
}

Socket::Socket(std::string_view domain, std::string_view socket_type, std::string_view protocol): Socket()
{
    SIHD_UNEXPECTED_LOG(this->open(domain, socket_type, protocol));
}

Socket::Socket(int socket, bool get_infos): Socket()
{
    if (socket >= 0)
    {
        _socket = socket;
        if (get_infos)
            (void)this->get_infos();
    }
}

Socket::Socket(int socket, int domain, int socket_type, int protocol): Socket()
{
    _socket = socket;
    _domain = domain;
    _type = socket_type;
    _protocol = protocol;
}

Socket::Socket(int socket, std::string_view domain, std::string_view socket_type, std::string_view protocol): Socket()
{
    _socket = socket;
    _domain = ip::domain(domain);
    _type = ip::socktype(socket_type);
    _protocol = ip::protocol(protocol);
    if (_domain == -1)
        SIHD_LOG(error, "Socket: domain unknown: {}", domain);
    if (_type == -1)
        SIHD_LOG(error, "Socket: socket type unknown: {}", socket_type);
    if (_protocol == -1)
        SIHD_LOG(error, "Socket: protocol unknown: {}", protocol);
}

Socket::Socket(Socket && other): Socket()
{
    *this = std::move(other);
}

Socket & Socket::operator=(Socket && other)
{
    if (this != &other)
    {
        SIHD_UNEXPECTED_LOG(this->shutdown());
        SIHD_UNEXPECTED_LOG(this->close());
        _socket = other._socket;
        _domain = other._domain;
        _type = other._type;
        _protocol = other._protocol;
        _verbose = other._verbose;
        _send_flags = other._send_flags;
        _rcv_flags = other._rcv_flags;
        _unix_bind_path = std::move(other._unix_bind_path);
        _connect_addr = std::move(other._connect_addr);
        _connect_unix_path = std::move(other._connect_unix_path);
        _blocking = other._blocking;

        other._clear_socket_info();
        other._domain = 0;
        other._type = 0;
        other._protocol = 0;
    }
    return *this;
}

Socket::~Socket()
{
    (void)this->shutdown();
    (void)this->close();
}

void Socket::_clear_socket_info()
{
    _socket = -1;
    _blocking = true;
}

/* ************************************************************************* */
/* Static class utilities */
/* ************************************************************************* */

Error Socket::make_error(std::string_view message)
{
    const int err = sihd::sys::os::last_socket_error();
    return Error(sihd::util::error_errno(err), "{}: {}", message, sihd::sys::os::error_str(err));
}

std::expected<void, Error> Socket::set_socket_ttl(int socket, int ttl, bool ipv6)
{
    return opt_result("set ttl",
                      sihd::sys::os::setsockopt(socket,
                                                ipv6 ? IPPROTO_IPV6 : IPPROTO_IP,
                                                ipv6 ? IPV6_UNICAST_HOPS : IP_TTL,
                                                &ttl,
                                                sizeof(int)));
}

std::expected<void, Error> Socket::set_socket_reuseaddr(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set reuseaddr", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)));
}

std::expected<void, Error> Socket::set_socket_broadcast(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set broadcast", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(int)));
}

std::expected<void, Error> Socket::set_socket_tcp_nodelay(int socket, bool active)
{
    int opt = (active ? 1 : 0);
    return opt_result("set tcp nodelay",
                      sihd::sys::os::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt), true));
}

bool Socket::is_socket_tcp_nodelay(int socket)
{
    int opt;
    socklen_t len = sizeof(opt);
    return sihd::sys::os::getsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, &len, true) && opt != 0;
}

bool Socket::is_socket_broadcast(int socket)
{
    int res;
    socklen_t length = sizeof(int);
    return sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_BROADCAST, &res, &length, true) && res != 0;
}

std::expected<void, Error> Socket::set_socket_keepalive(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set keepalive", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(int)));
}

bool Socket::is_socket_keepalive(int socket)
{
#if defined(__SIHD_WINDOWS__)
    // Winsock getsockopt(SO_KEEPALIVE) writes a single byte boolean, not a 4-byte int
    char opt = 0;
#else
    int opt = 0;
#endif
    socklen_t len = sizeof(opt);
    return sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &opt, &len, true) && opt != 0;
}

#ifdef SO_REUSEPORT
std::expected<void, Error> Socket::set_socket_reuseport(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set reuseport", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)));
}

bool Socket::is_socket_reuseport(int socket)
{
    int opt;
    socklen_t len = sizeof(opt);
    return sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_REUSEPORT, &opt, &len, true) && opt != 0;
}
#endif

std::expected<void, Error> Socket::set_socket_rcvbuf(int socket, int size)
{
    return opt_result("set rcvbuf", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)));
}

std::expected<void, Error> Socket::set_socket_sndbuf(int socket, int size)
{
    return opt_result("set sndbuf", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)));
}

int Socket::get_socket_rcvbuf(int socket)
{
    int val = -1;
    socklen_t len = sizeof(val);
    sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &val, &len, true);
    return val;
}

int Socket::get_socket_sndbuf(int socket)
{
    int val = -1;
    socklen_t len = sizeof(val);
    sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_SNDBUF, &val, &len, true);
    return val;
}

std::optional<int> Socket::get_socket_error(int socket)
{
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_ERROR, &so_error, &len) == false)
        return std::nullopt;
    return so_error;
}

std::expected<void, Error> Socket::get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: cannot get peer name on a negative socket"));
    return opt_result("get peer name", ::getpeername(socket, addr, addr_len) == 0);
}

std::optional<IpAddr> Socket::socket_ip(int socket, bool ipv6)
{
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    socklen_t len;
    if (ipv6)
    {
        len = sizeof(addr_in6);
        if (Socket::get_socket_peername(socket, (sockaddr *)&addr_in6, &len))
            return IpAddr(addr_in6);
    }
    else
    {
        len = sizeof(addr_in);
        if (Socket::get_socket_peername(socket, (sockaddr *)&addr_in, &len))
            return IpAddr(addr_in);
    }
    return std::nullopt;
}

std::optional<IpAddr> Socket::peeraddr(bool ipv6) const
{
    return Socket::socket_ip(_socket, ipv6);
}

int Socket::local_port() const
{
    sockaddr_in6 addr6;
    socklen_t len = sizeof(addr6);
    if (::getsockname(_socket, (sockaddr *)&addr6, &len) != 0)
        return -1;
    if (addr6.sin6_family == AF_INET6)
        return ntohs(addr6.sin6_port);
    sockaddr_in *addr4 = reinterpret_cast<sockaddr_in *>(&addr6);
    if (addr4->sin_family == AF_INET)
        return ntohs(addr4->sin_port);
    return -1;
}

/* ************************************************************************* */
/* Socket open/close */
/* ************************************************************************* */

bool Socket::get_infos()
{
    return _socket >= 0 && Socket::get_socket_infos(_socket, &_domain, &_type, &_protocol);
}

std::expected<void, Error> Socket::set_tcp_nodelay(bool active) const
{
    return Socket::set_socket_tcp_nodelay(_socket, active);
}
std::expected<void, Error> Socket::set_blocking(bool active) const
{
    auto res = Socket::set_socket_blocking(_socket, active);
    if (res)
        _blocking = active;
    return res;
}
std::expected<void, Error> Socket::set_recv_timeout(int milliseconds) const
{
    return Socket::set_socket_recv_timeout(_socket, milliseconds);
}
std::expected<void, Error> Socket::set_reuseaddr(bool active) const
{
    return Socket::set_socket_reuseaddr(_socket, active);
}
std::expected<void, Error> Socket::set_broadcast(bool active) const
{
    return Socket::set_socket_broadcast(_socket, active);
}
std::expected<void, Error> Socket::bind_to_device(std::string_view name) const
{
    return Socket::bind_socket_to_device(_socket, name);
}
bool Socket::is_tcp_nodelay() const
{
    return Socket::is_socket_tcp_nodelay(_socket);
}
bool Socket::is_blocking() const
{
    if (_socket < 0)
        return false;
#if defined(__SIHD_WINDOWS__)
    // winsock cannot report the mode: the tracked state is authoritative
    return _blocking;
#else
    return Socket::is_socket_blocking(_socket);
#endif
}
bool Socket::is_broadcast() const
{
    return Socket::is_socket_broadcast(_socket);
}
std::expected<void, Error> Socket::set_ttl(int ttl) const
{
    return Socket::set_socket_ttl(_socket, ttl, this->is_ipv6());
}
std::expected<void, Error> Socket::set_keepalive(bool active) const
{
    return Socket::set_socket_keepalive(_socket, active);
}
bool Socket::is_keepalive() const
{
    return Socket::is_socket_keepalive(_socket);
}
#ifdef SO_REUSEPORT
std::expected<void, Error> Socket::set_reuseport(bool active) const
{
    return Socket::set_socket_reuseport(_socket, active);
}
bool Socket::is_reuseport() const
{
    return Socket::is_socket_reuseport(_socket);
}
#endif
std::expected<void, Error> Socket::set_rcvbuf(int size) const
{
    return Socket::set_socket_rcvbuf(_socket, size);
}
std::expected<void, Error> Socket::set_sndbuf(int size) const
{
    return Socket::set_socket_sndbuf(_socket, size);
}
int Socket::get_rcvbuf() const
{
    return Socket::get_socket_rcvbuf(_socket);
}
int Socket::get_sndbuf() const
{
    return Socket::get_socket_sndbuf(_socket);
}

std::expected<void, Error> Socket::open(std::string_view domain, std::string_view type, std::string_view protocol)
{
    int sockdomain = ip::domain(domain);
    int socktype = ip::socktype(type);
    int sockprotocol = ip::protocol(protocol);
    if (sockdomain >= 0 && socktype >= 0 && sockprotocol >= 0)
        return this->open(sockdomain, socktype, sockprotocol);
    return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: unknown domain, type or protocol"));
}

std::expected<void, Error> Socket::open(int domain, int type, int protocol)
{
    if (this->is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "Socket: socket already open"));
    _socket = ::socket(domain, type, protocol);
    _domain = domain;
    _type = type;
    _protocol = protocol;
    if (_socket < 0)
        return std::unexpected(make_error("Socket: open error"));
    return {};
}

std::expected<void, Error> Socket::close_socket(int socket)
{
#if defined(__SIHD_WINDOWS__)
    return opt_result("close", ::closesocket(socket) == 0);
#else
    return opt_result("close", ::close(socket) == 0);
#endif
}

std::expected<void, Error> Socket::close()
{
    if (_socket < 0)
        return {};
    std::expected<void, Error> res = Socket::close_socket(_socket);
    if (!_unix_bind_path.empty())
    {
        SIHD_UNEXPECTED_LOG(sihd::sys::fs::remove_file(_unix_bind_path));
        _unix_bind_path.clear();
    }
    this->_clear_socket_info();
    return res;
}

std::expected<void, Error> Socket::shutdown() const
{
    if (_socket < 0)
        return {};
    if (::shutdown(_socket, SHUT_RDWR) == 0)
        return {};
    return std::unexpected(make_error("Socket: shutdown error"));
}

/* ************************************************************************* */
/* Socket sockaddr operations */
/* ************************************************************************* */

std::expected<int, Error> Socket::accept(sockaddr *addr, socklen_t *addr_len, int timeout_ms)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("accept"));
    if (timeout_ms >= 0)
    {
        sihd::sys::Poll poll;
        poll.set_limit(1);
        poll.set_read_fd(_socket);
        poll.poll(timeout_ms);
        if (poll.polling_error())
            return std::unexpected(make_error("Socket: accept poll error"));
        if (poll.polling_timeout())
            return std::unexpected(Error(ErrorCode::timeout, "Socket: accept timeout after {}ms", timeout_ms));
    }
    return size_result(socket_call("accept", [&] { return (int)::accept(_socket, addr, addr_len); }));
}

std::expected<void, Error> Socket::listen(uint16_t queue_size)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("listen"));
    return opt_result("listen", ::listen(_socket, queue_size) != -1);
}

std::expected<void, Error> Socket::bind(const sockaddr *addr, socklen_t addr_len)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("bind"));
    return opt_result("bind", ::bind(_socket, addr, addr_len) != -1);
}

std::expected<void, Error> Socket::connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("connect"));
    bool use_timeout = (timeout_ms >= 0);
    bool was_blocking = false;
    if (use_timeout)
    {
        was_blocking = this->is_blocking();
        if (was_blocking)
            (void)this->set_blocking(false);
    }
    // the slot is clobbered by any win32 call, message formatting included: read once per syscall
    std::expected<void, Error> res = {};
    const int err = ::connect(_socket, addr, addr_len) == 0 ? 0 : sihd::sys::os::last_socket_error();
    if (err != 0)
    {
#if !defined(__SIHD_WINDOWS__)
        bool in_progress = (err == EINPROGRESS || err == EALREADY);
        bool already_connected = (err == EISCONN);
#else
        bool in_progress = (err == WSAEWOULDBLOCK || err == WSAEALREADY);
        bool already_connected = (err == WSAEISCONN);
#endif
        if (already_connected)
        {
            res = {};
        }
        else if (in_progress && use_timeout)
        {
            sihd::sys::Poll poll;
            poll.set_limit(1);
            poll.set_write_fd(_socket);
            poll.poll(timeout_ms);
            if (poll.polling_error())
                res = std::unexpected(make_error("Socket: connect poll error"));
            else if (poll.polling_timeout())
                res = std::unexpected(Error(ErrorCode::timeout, "Socket: connect timeout after {}ms", timeout_ms));
            else
            {
                const std::optional<int> so_error = this->get_error();
                if (so_error == std::nullopt)
                    res = std::unexpected(make_error("Socket: connect error"));
                else if (*so_error != 0)
                    res = std::unexpected(Error(sihd::util::error_errno(*so_error),
                                                "Socket: connect error: {}",
                                                sihd::sys::os::error_str(*so_error)));
                else
                    res = {};
            }
        }
        else
            res = std::unexpected(
                Error(sihd::util::error_errno(err), "Socket: connect error: {}", sihd::sys::os::error_str(err)));
    }
    if (use_timeout && was_blocking)
        (void)this->set_blocking(true);
    return res;
}

std::optional<int> Socket::get_error() const
{
    return Socket::get_socket_error(_socket);
}

std::string Socket::get_error_str() const
{
    const std::optional<int> so_error = this->get_error();
    if (so_error == std::nullopt)
        return "";
    return sihd::sys::os::error_str(*so_error);
}

std::expected<void, Error> Socket::reconnect(int timeout_ms)
{
    int domain = _domain;
    int type = _type;
    int protocol = _protocol;
    bool unix = this->is_unix();
    (void)this->shutdown();
    auto closed = this->close();
    if (!closed)
        return closed;
    auto opened = this->open(domain, type, protocol);
    if (!opened)
        return opened;
    if (unix)
        return this->connect_unix(_connect_unix_path);
    if (_connect_addr.empty())
        return std::unexpected(Error(ErrorCode::not_initialized, "Socket: no address to reconnect to"));
    return this->connect(_connect_addr, timeout_ms);
}

std::expected<size_t, Error> Socket::send(sihd::util::ArrCharView view)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("send"));
    return size_result(socket_call("send", [&] {
#if !defined(__SIHD_WINDOWS__)
        return ::send(_socket, view.data(), view.size(), _send_flags);
#else
        return ::send(_socket, (const char *)view.data(), view.size(), _send_flags);
#endif
    }));
}

std::expected<void, Error> Socket::send_all(sihd::util::ArrCharView view)
{
    // datagrams are atomic: the remainder would be a second datagram
    if (atomic_datagram_type(_type))
    {
        auto sent = this->send(view);
        SIHD_UNEXPECTED_RETURN(sent);
        if (*sent != view.size())
            return std::unexpected(Error(ErrorCode::io_error, "Socket: datagram sent partially"));
        return {};
    }
    size_t sent = 0;
    while (sent < view.size())
    {
        auto ret = this->send({view.data() + sent, view.size() - sent});
        SIHD_UNEXPECTED_RETURN(ret);
        if (*ret == 0)
            return std::unexpected(Error(ErrorCode::io_error, "Socket: send made no progress"));
        sent += *ret;
    }
    return {};
}

std::expected<size_t, Error> Socket::receive(void *data, size_t size)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("receive"));
    return size_result(socket_call("receive", [&] {
#if !defined(__SIHD_WINDOWS__)
        return ::recv(_socket, data, size, _rcv_flags);
#else
        return ::recv(_socket, (char *)data, size, _rcv_flags);
#endif
    }));
}

std::expected<size_t, Error> Socket::receive(sihd::util::IArray & arr)
{
    auto res = this->receive(arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

std::expected<size_t, Error> Socket::send_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("send_to"));
    return size_result(socket_call("send_to", [&] {
#if !defined(__SIHD_WINDOWS__)
        return ::sendto(_socket, view.data(), view.size(), _send_flags, addr, addr_len);
#else
        return ::sendto(_socket, (const char *)view.data(), view.size(), _send_flags, addr, addr_len);
#endif
    }));
}

std::expected<void, Error> Socket::send_all_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view)
{
    // datagrams are atomic: the remainder would be a second datagram
    if (atomic_datagram_type(_type))
    {
        auto sent = this->send_to(addr, addr_len, view);
        SIHD_UNEXPECTED_RETURN(sent);
        if (*sent != view.size())
            return std::unexpected(Error(ErrorCode::io_error, "Socket: datagram sent partially"));
        return {};
    }
    size_t sent = 0;
    while (sent < view.size())
    {
        auto ret = this->send_to(addr, addr_len, {view.data() + sent, view.size() - sent});
        SIHD_UNEXPECTED_RETURN(ret);
        if (*ret == 0)
            return std::unexpected(Error(ErrorCode::io_error, "Socket: send made no progress"));
        sent += *ret;
    }
    return {};
}

std::expected<size_t, Error> Socket::receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("receive_from"));
    return size_result(socket_call("receive_from", [&] {
#if !defined(__SIHD_WINDOWS__)
        return ::recvfrom(_socket, data, size, _rcv_flags, addr, addr_len);
#else
        return ::recvfrom(_socket, (char *)data, size, _rcv_flags, addr, addr_len);
#endif
    }));
}

std::expected<size_t, Error> Socket::receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr)
{
    auto res = this->receive_from(addr, addr_len, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

std::expected<size_t, Error> Socket::receive_from(IpAddr & addr, sihd::util::IArray & arr)
{
    auto res = this->receive_from(addr, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

std::expected<size_t, Error> Socket::receive_from_unix(std::string & path, sihd::util::IArray & arr)
{
    auto res = this->receive_from_unix(path, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

bool Socket::is_unix() const
{
    return _domain == AF_UNIX;
}

bool Socket::is_ip() const
{
    return this->is_ipv4() || this->is_ipv6();
}

bool Socket::is_ipv4() const
{
    return _domain == AF_INET;
}

bool Socket::is_ipv6() const
{
    return _domain == AF_INET6;
}

/* ************************************************************************* */
/* Socket IpAddr operations */
/* ************************************************************************* */

std::expected<size_t, Error> Socket::send_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return this->send_to(&addr.addr(), addr.addr_len(), view);
}

std::expected<void, Error> Socket::send_all_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return this->send_all_to(&addr.addr(), addr.addr_len(), view);
}

std::expected<size_t, Error> Socket::receive_from(IpAddr & ipaddr, void *data, size_t size)
{
    sockaddr *addr;
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    socklen_t len;
    if (_domain == AF_INET6)
    {
        addr = (sockaddr *)&addr_in6;
        len = sizeof(struct sockaddr_in6);
    }
    else
    {
        addr = (sockaddr *)&addr_in;
        len = sizeof(struct sockaddr_in);
    }
    auto res = this->receive_from(addr, &len, data, size);
    if (res)
        ipaddr = IpAddr(*addr, len);
    return res;
}

std::expected<void, Error> Socket::bind(const IpAddr & addr)
{
    return this->bind(&addr.addr(), addr.addr_len());
}

std::expected<void, Error> Socket::connect(const IpAddr & addr, int timeout_ms)
{
    auto res = this->connect(&addr.addr(), addr.addr_len(), timeout_ms);
    if (res)
    {
        _connect_addr = addr;
        _connect_unix_path.clear();
    }
    return res;
}

std::expected<int, Error> Socket::accept(IpAddr & ipaddr, int timeout_ms)
{
    sockaddr *addr;
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    socklen_t len;
    if (_domain == AF_INET6)
    {
        addr = (sockaddr *)&addr_in6;
        len = sizeof(struct sockaddr_in6);
    }
    else
    {
        addr = (sockaddr *)&addr_in;
        len = sizeof(struct sockaddr_in);
    }
    auto res = this->accept(addr, &len, timeout_ms);
    if (res)
        ipaddr = IpAddr(*addr, len);
    return res;
}

std::expected<int, Error> Socket::accept(int timeout_ms)
{
    return this->accept(nullptr, nullptr, timeout_ms);
}

/* ************************************************************************* */
/* Socket multicast */
/* ************************************************************************* */

std::expected<void, Error>
    Socket::_multicast_membership(const IpAddr & group, std::string_view iface, int ipv4_opt, int ipv6_opt)
{
    if (!this->is_open())
        return std::unexpected(closed_socket_error("multicast membership"));
    if (group.is_ipv4())
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = group.addr4().sin_addr;
        if (iface.empty())
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        else
        {
            IpAddr iface_addr(iface);
            mreq.imr_interface = iface_addr.addr4().sin_addr;
        }
        return opt_result("multicast membership",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IP, ipv4_opt, &mreq, sizeof(mreq)));
    }
    else if (group.is_ipv6())
    {
        struct ipv6_mreq mreq;
        mreq.ipv6mr_multiaddr = group.addr6().sin6_addr;
        mreq.ipv6mr_interface = 0;
#if !defined(__SIHD_WINDOWS__)
        if (!iface.empty())
        {
            mreq.ipv6mr_interface = if_nametoindex(iface.data());
            if (mreq.ipv6mr_interface == 0)
                return std::unexpected(Error(ErrorCode::not_found, "Socket: no interface named: {}", iface));
        }
#endif
        return opt_result("multicast membership",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IPV6, ipv6_opt, &mreq, sizeof(mreq)));
    }
    return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: multicast group is not an ip address"));
}

std::expected<void, Error> Socket::join_multicast(const IpAddr & group, std::string_view iface)
{
    return this->_multicast_membership(group, iface, IP_ADD_MEMBERSHIP, IPV6_JOIN_GROUP);
}

std::expected<void, Error> Socket::leave_multicast(const IpAddr & group, std::string_view iface)
{
    return this->_multicast_membership(group, iface, IP_DROP_MEMBERSHIP, IPV6_LEAVE_GROUP);
}

std::expected<void, Error> Socket::set_multicast_ttl(int ttl)
{
    if (!this->is_open())
        return std::unexpected(closed_socket_error("set multicast ttl"));
    if (_domain == AF_INET6)
        return opt_result("set multicast ttl",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)));
    return opt_result("set multicast ttl",
                      sihd::sys::os::setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)));
}

std::expected<void, Error> Socket::set_multicast_loop(bool active)
{
    if (!this->is_open())
        return std::unexpected(closed_socket_error("set multicast loop"));
    if (_domain == AF_INET6)
    {
        int val = active ? 1 : 0;
        return opt_result("set multicast loop",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val)));
    }
    int val = active ? 1 : 0;
    return opt_result("set multicast loop",
                      sihd::sys::os::setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)));
}

/* ************************************************************************* */
/* Socket UNIX operations */
/* ************************************************************************* */

std::expected<void, Error> Socket::bind_unix(std::string_view path)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: invalid unix path: {}", path));
    auto res = this->bind((sockaddr *)&addr, *addr_len);
    if (res)
    {
        // an abstract socket owns no filesystem name: nothing to unlink
        if (path[0] != '\0')
            _unix_bind_path = path;
    }
    return res;
}

std::expected<void, Error> Socket::connect_unix(std::string_view path)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: invalid unix path: {}", path));
    auto res = this->connect((sockaddr *)&addr, *addr_len);
    if (res)
    {
        _connect_unix_path = path;
        _connect_addr = IpAddr();
    }
    return res;
}

std::expected<size_t, Error> Socket::send_to_unix(std::string_view path, sihd::util::ArrCharView view)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: invalid unix path: {}", path));
    return this->send_to((sockaddr *)&addr, *addr_len, view);
}

std::expected<void, Error> Socket::send_all_to_unix(std::string_view path, sihd::util::ArrCharView view)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Socket: invalid unix path: {}", path));
    return this->send_all_to((sockaddr *)&addr, *addr_len, view);
}

std::expected<size_t, Error> Socket::receive_from_unix(std::string & path, void *data, size_t size)
{
    sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = sizeof(sockaddr_un);
    auto res = this->receive_from((sockaddr *)&addr, &addr_len, data, size);
    if (res && *res > 0)
        path = unix_path_from_addr(addr, addr_len);
    return res;
}

std::string Socket::unix_socket_peername(int socket)
{
    sockaddr_un addr_un;
    memset(&addr_un, 0, sizeof(addr_un));
    socklen_t len = sizeof(addr_un);
    if (Socket::get_socket_peername(socket, (sockaddr *)&addr_un, &len))
        return unix_path_from_addr(addr_un, len);
    return "";
}

/* ************************************************************************* */
/* Socket utilities operations */
/* ************************************************************************* */

} // namespace sihd::net
