#include <unistd.h>

#include <cstring>
#include <expected>

#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/tcp.h>
# include <sys/un.h>

# include <net/if.h>
#else
# include <afunix.h>
# include <ws2tcpip.h>

#endif

using namespace sihd::util;

namespace sihd::net
{

namespace
{

void adapt_array_size(IArray & arr, size_t sent)
{
    if (sent <= arr.byte_capacity() && sent != arr.byte_size())
        arr.byte_resize(sent);
}

bool interrupted_error(int err)
{
    // winsock operations are not interrupted by signals
    return !build::is_windows && err == EINTR;
}

template <typename Syscall>
auto retry_interrupted(Syscall && syscall)
{
    auto ret = syscall();
    while (ret < 0 && interrupted_error(sihd::sys::os::last_socket_error()))
        ret = syscall();
    return ret;
}

Result<ssize_t> socket_call(std::string_view op, auto && syscall)
{
    auto ret = retry_interrupted(syscall);
    if (ret >= 0)
        return ret;

    const int err = sihd::sys::os::last_socket_error();
    return std::unexpected(Error(error_errno(err), "{} error: {}", op, sihd::sys::os::error_str(err)));
}

Result<size_t> size_result(Result<ssize_t> && res)
{
    SIHD_UNEXPECTED_RETURN(res);
    return (size_t)*res;
}

Result<void> opt_result(std::string_view op, Result<void> res)
{
    SIHD_UNEXPECTED_RETURN_CTX(res, "{}", op);
    return {};
}

Result<void> opt_result(std::string_view op, bool ok)
{
    if (ok)
        return {};

    const int err = sihd::sys::os::last_socket_error();
    return std::unexpected(Error(error_errno(err), "{} error: {}", op, sihd::sys::os::error_str(err)));
}

Error closed_socket_error(std::string_view op)
{
    return Error(ErrorCode::closed, "cannot {} on a closed socket", op);
}

Result<int> get_int_option(int socket, int level, int option, std::string_view op)
{
    int value = 0;
    socklen_t len = sizeof(value);

    auto res = sihd::sys::os::getsockopt(socket, level, option, &value, &len);
    SIHD_UNEXPECTED_RETURN_CTX(res, "{}", op);
    return value;
}

Error os_error(std::string_view message)
{
    const int err = sihd::sys::os::last_socket_error();
    return Error(error_errno(err), "{}: {}", message, sihd::sys::os::error_str(err));
}

bool connect_in_progress([[maybe_unused]] int err)
{
#if !defined(__SIHD_WINDOWS__)
    return err == EINPROGRESS || err == EALREADY;
#else
    return err == WSAEWOULDBLOCK || err == WSAEALREADY;
#endif
}

bool connect_already_done([[maybe_unused]] int err)
{
#if !defined(__SIHD_WINDOWS__)
    return err == EISCONN;
#else
    return err == WSAEISCONN;
#endif
}

Result<void> wait_connect(const Socket & socket, int timeout_ms)
{
    sihd::sys::Poll poll;
    poll.set_limit(1);
    poll.set_write_fd(socket.socket());
    poll.poll(timeout_ms);

    if (poll.polling_error())
        return std::unexpected(os_error("connect poll error"));
    if (poll.polling_timeout())
        return std::unexpected(Error(ErrorCode::timeout, "connect timeout after {}ms", timeout_ms));

    Result<int> so_error = socket.get_error();
    SIHD_UNEXPECTED_RETURN(so_error);
    if (*so_error != 0)
        return std::unexpected(Error(error_errno(*so_error), "connect error: {}", sihd::sys::os::error_str(*so_error)));
    return {};
}

Result<void> connect_outcome(const Socket & socket, int err, bool use_timeout, int timeout_ms)
{
    if (connect_already_done(err))
        return {};
    if (connect_in_progress(err) && use_timeout)
        return wait_connect(socket, timeout_ms);
    return std::unexpected(Error(error_errno(err), "connect error: {}", sihd::sys::os::error_str(err)));
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
    if (path.empty())
        return std::nullopt;

    // addr_len separates pathname (terminator included) from abstract (leading null kept)
    const size_t terminator = (path[0] != '\0');
    if (path.size() + terminator > sizeof(addr->sun_path))
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
#ifdef MSG_NOSIGNAL
    _send_flags = MSG_NOSIGNAL;
#else
    _send_flags = 0;
#endif
}

Socket::Socket(int domain, int socket_type, int protocol): Socket()
{
    auto opened = this->open(domain, socket_type, protocol);
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

Socket::Socket(std::string_view domain, std::string_view socket_type, std::string_view protocol): Socket()
{
    auto opened = this->open(domain, socket_type, protocol);
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

Socket::Socket(int socket, bool get_infos): Socket()
{
    if (socket < 0)
        return;
    if (get_infos)
    {
        // queried before adopting: a throw leaves the descriptor to the caller
        auto infos = Socket::get_socket_infos(socket, &_domain, &_type, &_protocol);
        if (!infos)
            throw std::runtime_error(infos.error().message);
    }
    _socket = socket;
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
    _domain = ip::domain(domain);
    _type = ip::socktype(socket_type);
    _protocol = ip::protocol(std::string(protocol));

    if (_domain == -1)
        throw std::invalid_argument(fmt::format("socket domain unknown: {}", domain));
    if (_type == -1)
        throw std::invalid_argument(fmt::format("socket type unknown: {}", socket_type));
    if (_protocol == -1)
        throw std::invalid_argument(fmt::format("socket protocol unknown: {}", protocol));

    _socket = socket;
}

Socket::Socket(Socket && other): Socket()
{
    *this = std::move(other);
}

Socket & Socket::operator=(Socket && other)
{
    if (this != &other)
    {
        (void)this->shutdown();
        (void)this->close();

        _socket = other._socket;
        _domain = other._domain;
        _type = other._type;
        _protocol = other._protocol;
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

Socket::operator int() const
{
    return _socket;
}

Socket::operator bool() const
{
    return this->is_open();
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
    return os_error(message);
}

Result<void> Socket::set_socket_ttl(int socket, int ttl, bool ipv6)
{
    return opt_result("set ttl",
                      sihd::sys::os::setsockopt(socket,
                                                ipv6 ? IPPROTO_IPV6 : IPPROTO_IP,
                                                ipv6 ? IPV6_UNICAST_HOPS : IP_TTL,
                                                &ttl,
                                                sizeof(int)));
}

Result<void> Socket::set_socket_reuseaddr(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set reuseaddr", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)));
}

Result<void> Socket::set_socket_broadcast(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set broadcast", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(int)));
}

Result<void> Socket::set_socket_tcp_nodelay(int socket, bool active)
{
    int opt = (active ? 1 : 0);
    return opt_result("set tcp nodelay",
                      sihd::sys::os::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)));
}

Result<bool> Socket::is_socket_tcp_nodelay(int socket)
{
    auto opt = get_int_option(socket, IPPROTO_TCP, TCP_NODELAY, "get tcp nodelay");
    SIHD_UNEXPECTED_RETURN(opt);
    return *opt != 0;
}

Result<bool> Socket::is_socket_broadcast(int socket)
{
    auto opt = get_int_option(socket, SOL_SOCKET, SO_BROADCAST, "get broadcast");
    SIHD_UNEXPECTED_RETURN(opt);
    return *opt != 0;
}

Result<void> Socket::set_socket_keepalive(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return opt_result("set keepalive", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(int)));
}

Result<bool> Socket::is_socket_keepalive(int socket)
{
    // winsock getsockopt(SO_KEEPALIVE) writes a single byte boolean, not a 4-byte int
    std::conditional_t<build::is_windows, char, int> opt = 0;
    socklen_t len = sizeof(opt);

    auto res = sihd::sys::os::getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &opt, &len);
    SIHD_UNEXPECTED_RETURN_CTX(res, "get keepalive");
    return opt != 0;
}

Result<void> Socket::set_socket_reuseport([[maybe_unused]] int socket, [[maybe_unused]] bool active)
{
#ifdef SO_REUSEPORT
    int opt = active ? 1 : 0;
    return opt_result("set reuseport", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)));
#else
    return std::unexpected(Error(ErrorCode::not_supported, "set reuseport: SO_REUSEPORT not supported"));
#endif
}

Result<bool> Socket::is_socket_reuseport([[maybe_unused]] int socket)
{
#ifdef SO_REUSEPORT
    auto opt = get_int_option(socket, SOL_SOCKET, SO_REUSEPORT, "get reuseport");
    SIHD_UNEXPECTED_RETURN(opt);
    return *opt != 0;
#else
    return std::unexpected(Error(ErrorCode::not_supported, "get reuseport: SO_REUSEPORT not supported"));
#endif
}

Result<void> Socket::set_socket_rcvbuf(int socket, int size)
{
    return opt_result("set rcvbuf", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)));
}

Result<void> Socket::set_socket_sndbuf(int socket, int size)
{
    return opt_result("set sndbuf", sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)));
}

Result<int> Socket::get_socket_rcvbuf(int socket)
{
    return get_int_option(socket, SOL_SOCKET, SO_RCVBUF, "get rcvbuf");
}

Result<int> Socket::get_socket_sndbuf(int socket)
{
    return get_int_option(socket, SOL_SOCKET, SO_SNDBUF, "get sndbuf");
}

Result<int> Socket::get_socket_error(int socket)
{
    return get_int_option(socket, SOL_SOCKET, SO_ERROR, "get socket error");
}

Result<void> Socket::get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot get peer name on a negative socket"));
    return opt_result("get peer name", ::getpeername(socket, addr, addr_len) == 0);
}

Result<IpAddr> Socket::socket_ip(int socket, bool ipv6)
{
    if (ipv6)
    {
        sockaddr_in6 addr_in6;
        socklen_t len = sizeof(addr_in6);
        auto peer = Socket::get_socket_peername(socket, (sockaddr *)&addr_in6, &len);
        SIHD_UNEXPECTED_RETURN(peer);
        return IpAddr(addr_in6);
    }

    sockaddr_in addr_in;
    socklen_t len = sizeof(addr_in);
    auto peer = Socket::get_socket_peername(socket, (sockaddr *)&addr_in, &len);
    SIHD_UNEXPECTED_RETURN(peer);
    return IpAddr(addr_in);
}

Result<IpAddr> Socket::peeraddr(bool ipv6) const
{
    return Socket::socket_ip(_socket, ipv6);
}

Result<int> Socket::local_port() const
{
    sockaddr_in6 addr6;
    socklen_t len = sizeof(addr6);

    if (::getsockname(_socket, (sockaddr *)&addr6, &len) != 0)
    {
        const int err = sihd::sys::os::last_socket_error();
        return std::unexpected(Error(error_errno(err), "local port error: {}", sihd::sys::os::error_str(err)));
    }

    if (addr6.sin6_family == AF_INET6)
        return ntohs(addr6.sin6_port);

    sockaddr_in *addr4 = reinterpret_cast<sockaddr_in *>(&addr6);
    if (addr4->sin_family == AF_INET)
        return ntohs(addr4->sin_port);
    return std::unexpected(Error(ErrorCode::not_supported, "local port: not an ip socket"));
}

/* ************************************************************************* */
/* Socket open/close */
/* ************************************************************************* */

Result<void> Socket::get_infos()
{
    if (_socket < 0)
        return std::unexpected(closed_socket_error("get infos"));
    return Socket::get_socket_infos(_socket, &_domain, &_type, &_protocol);
}

Result<void> Socket::set_tcp_nodelay(bool active) const
{
    return Socket::set_socket_tcp_nodelay(_socket, active);
}

Result<void> Socket::set_blocking(bool active) const
{
    auto res = Socket::set_socket_blocking(_socket, active);
    if (res)
        _blocking = active;
    return res;
}

Result<void> Socket::set_recv_timeout(int milliseconds) const
{
    return Socket::set_socket_recv_timeout(_socket, milliseconds);
}

Result<void> Socket::set_reuseaddr(bool active) const
{
    return Socket::set_socket_reuseaddr(_socket, active);
}

Result<void> Socket::set_broadcast(bool active) const
{
    return Socket::set_socket_broadcast(_socket, active);
}

Result<void> Socket::bind_to_device(std::string_view name) const
{
    return Socket::bind_socket_to_device(_socket, name);
}

Result<bool> Socket::is_tcp_nodelay() const
{
    return Socket::is_socket_tcp_nodelay(_socket);
}

Result<bool> Socket::is_blocking() const
{
    if (_socket < 0)
        return std::unexpected(closed_socket_error("get blocking"));
    // winsock cannot report the mode: the tracked state is authoritative
    if constexpr (build::is_windows)
        return _blocking;
    return Socket::is_socket_blocking(_socket);
}

Result<bool> Socket::is_broadcast() const
{
    return Socket::is_socket_broadcast(_socket);
}

Result<void> Socket::set_ttl(int ttl) const
{
    return Socket::set_socket_ttl(_socket, ttl, this->is_ipv6());
}

Result<void> Socket::set_keepalive(bool active) const
{
    return Socket::set_socket_keepalive(_socket, active);
}

Result<bool> Socket::is_keepalive() const
{
    return Socket::is_socket_keepalive(_socket);
}

Result<void> Socket::set_reuseport(bool active) const
{
    return Socket::set_socket_reuseport(_socket, active);
}

Result<bool> Socket::is_reuseport() const
{
    return Socket::is_socket_reuseport(_socket);
}

Result<void> Socket::set_socket_exclusiveaddruse([[maybe_unused]] int socket, [[maybe_unused]] bool active)
{
#ifdef SO_EXCLUSIVEADDRUSE
    int opt = active ? 1 : 0;
    return opt_result("set exclusiveaddruse",
                      sihd::sys::os::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &opt, sizeof(int)));
#else
    return std::unexpected(Error(ErrorCode::not_supported, "set exclusiveaddruse: SO_EXCLUSIVEADDRUSE not supported"));
#endif
}

Result<void> Socket::set_exclusiveaddruse(bool active) const
{
    return Socket::set_socket_exclusiveaddruse(_socket, active);
}

Result<void> Socket::set_rcvbuf(int size) const
{
    return Socket::set_socket_rcvbuf(_socket, size);
}

Result<void> Socket::set_sndbuf(int size) const
{
    return Socket::set_socket_sndbuf(_socket, size);
}

Result<int> Socket::get_rcvbuf() const
{
    return Socket::get_socket_rcvbuf(_socket);
}

Result<int> Socket::get_sndbuf() const
{
    return Socket::get_socket_sndbuf(_socket);
}

Result<void> Socket::open(std::string_view domain, std::string_view type, std::string_view protocol)
{
    int sockdomain = ip::domain(domain);
    int socktype = ip::socktype(type);
    int sockprotocol = ip::protocol(std::string(protocol));
    if (sockdomain >= 0 && socktype >= 0 && sockprotocol >= 0)
        return this->open(sockdomain, socktype, sockprotocol);
    return std::unexpected(Error(ErrorCode::invalid_argument, "unknown domain, type or protocol"));
}

Result<void> Socket::open(int domain, int type, int protocol)
{
    if (this->is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "socket already open"));

    _socket = ::socket(domain, type, protocol);
    _domain = domain;
    _type = type;
    _protocol = protocol;
    if (_socket < 0)
        return std::unexpected(make_error("open error"));
    return {};
}

Result<void> Socket::close_socket(int socket)
{
#if defined(__SIHD_WINDOWS__)
    return opt_result("close", ::closesocket(socket) == 0);
#else
    return opt_result("close", ::close(socket) == 0);
#endif
}

Result<void> Socket::close()
{
    if (_socket < 0)
        return {};

    Result<void> closed = Socket::close_socket(_socket);
    Result<void> unlinked = {};
    if (!_unix_bind_path.empty())
    {
        unlinked = sihd::sys::fs::remove_file(_unix_bind_path);
        _unix_bind_path.clear();
    }
    this->_clear_socket_info();

    if (!closed)
        return closed;
    return unlinked;
}

Result<void> Socket::shutdown() const
{
    if (_socket < 0)
        return {};
    if (::shutdown(_socket, SHUT_RDWR) == 0)
        return {};
    return std::unexpected(make_error("shutdown error"));
}

bool Socket::is_open() const
{
    return _socket >= 0;
}

/* ************************************************************************* */
/* Socket sockaddr operations */
/* ************************************************************************* */

Result<int> Socket::accept(sockaddr *addr, socklen_t *addr_len, int timeout_ms)
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
            return std::unexpected(make_error("accept poll error"));
        if (poll.polling_timeout())
            return std::unexpected(Error(ErrorCode::timeout, "accept timeout after {}ms", timeout_ms));
    }

    return size_result(socket_call("accept", [&] { return (int)::accept(_socket, addr, addr_len); }));
}

Result<void> Socket::listen(uint16_t queue_size)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("listen"));
    return opt_result("listen", ::listen(_socket, queue_size) != -1);
}

Result<void> Socket::bind(const sockaddr *addr, socklen_t addr_len)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("bind"));
    return opt_result("bind", ::bind(_socket, addr, addr_len) != -1);
}

Result<void> Socket::connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("connect"));

    const bool use_timeout = (timeout_ms >= 0);
    bool was_blocking = false;
    if (use_timeout)
    {
        auto blocking = this->is_blocking();
        SIHD_UNEXPECTED_RETURN_CTX(blocking, "preparing non-blocking connect, timeout {}ms", timeout_ms);
        was_blocking = *blocking;
        if (was_blocking)
        {
            auto non_blocking = this->set_blocking(false);
            SIHD_UNEXPECTED_RETURN_CTX(non_blocking, "preparing non-blocking connect, timeout {}ms", timeout_ms);
        }
    }

    // the slot is clobbered by any win32 call, message formatting included: read once per syscall
    const int err = ::connect(_socket, addr, addr_len) == 0 ? 0 : sihd::sys::os::last_socket_error();
    Result<void> res = err == 0 ? Result<void> {} : connect_outcome(*this, err, use_timeout, timeout_ms);

    if (use_timeout && was_blocking)
    {
        auto restored = this->set_blocking(true);
        if (res && !restored)
            return restored;
    }
    return res;
}

Result<int> Socket::get_error() const
{
    return Socket::get_socket_error(_socket);
}

Result<void> Socket::reconnect(int timeout_ms)
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
        return std::unexpected(Error(ErrorCode::not_initialized, "no address to reconnect to"));
    return this->connect(_connect_addr, timeout_ms);
}

const IpAddr & Socket::connect_addr() const
{
    return _connect_addr;
}

std::string Socket::connect_unix_path() const
{
    return _connect_unix_path;
}

Result<size_t> Socket::send(ArrCharView view)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("send"));

    return size_result(socket_call("send", [&] { return ::send(_socket, view.data(), view.size(), _send_flags); }));
}

Result<void> Socket::send_all(ArrCharView view)
{
    // datagrams are atomic: the remainder would be a second datagram
    if (atomic_datagram_type(_type))
    {
        auto sent = this->send(view);
        SIHD_UNEXPECTED_RETURN_CTX(sent, "sending {} bytes", view.size());
        if (*sent != view.size())
            return std::unexpected(Error(ErrorCode::io_error, "datagram sent partially"));
        return {};
    }

    size_t sent = 0;
    while (sent < view.size())
    {
        auto ret = this->send({view.data() + sent, view.size() - sent});
        SIHD_UNEXPECTED_RETURN_CTX(ret, "sending {} of {} bytes", sent, view.size());
        if (*ret == 0)
            return std::unexpected(Error(ErrorCode::io_error, "send made no progress"));
        sent += *ret;
    }
    return {};
}

Result<size_t> Socket::receive(void *data, size_t size)
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

Result<size_t> Socket::receive(IArray & arr)
{
    auto res = this->receive(arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

Result<size_t> Socket::send_to(const sockaddr *addr, socklen_t addr_len, ArrCharView view)
{
    if (this->is_open() == false)
        return std::unexpected(closed_socket_error("send_to"));

    return size_result(socket_call("send_to", [&] {
        return ::sendto(_socket, view.data(), view.size(), _send_flags, addr, addr_len);
    }));
}

Result<void> Socket::send_all_to(const sockaddr *addr, socklen_t addr_len, ArrCharView view)
{
    // datagrams are atomic: the remainder would be a second datagram
    if (atomic_datagram_type(_type))
    {
        auto sent = this->send_to(addr, addr_len, view);
        SIHD_UNEXPECTED_RETURN_CTX(sent, "sending {} bytes to {}", view.size(), ip::to_str(addr));
        if (*sent != view.size())
            return std::unexpected(Error(ErrorCode::io_error, "datagram sent partially"));
        return {};
    }

    size_t sent = 0;
    while (sent < view.size())
    {
        auto ret = this->send_to(addr, addr_len, {view.data() + sent, view.size() - sent});
        SIHD_UNEXPECTED_RETURN_CTX(ret, "sending {} of {} bytes to {}", sent, view.size(), ip::to_str(addr));
        if (*ret == 0)
            return std::unexpected(Error(ErrorCode::io_error, "send made no progress"));
        sent += *ret;
    }
    return {};
}

Result<size_t> Socket::receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size)
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

Result<size_t> Socket::receive_from(sockaddr *addr, socklen_t *addr_len, IArray & arr)
{
    auto res = this->receive_from(addr, addr_len, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

Result<size_t> Socket::receive_from(IpAddr & addr, IArray & arr)
{
    auto res = this->receive_from(addr, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

Result<size_t> Socket::receive_from_unix(std::string & path, IArray & arr)
{
    auto res = this->receive_from_unix(path, arr.buf(), arr.byte_capacity());
    if (res)
        adapt_array_size(arr, *res);
    return res;
}

void Socket::set_send_flags(int flags)
{
    _send_flags = flags;
}

void Socket::set_receive_flags(int flags)
{
    _rcv_flags = flags;
}

int Socket::domain() const
{
    return _domain;
}

int Socket::type() const
{
    return _type;
}

int Socket::protocol() const
{
    return _protocol;
}

int Socket::socket() const
{
    return _socket;
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

Result<size_t> Socket::send_to(const IpAddr & addr, ArrCharView view)
{
    return this->send_to(&addr.addr(), addr.addr_len(), view);
}

Result<void> Socket::send_all_to(const IpAddr & addr, ArrCharView view)
{
    return this->send_all_to(&addr.addr(), addr.addr_len(), view);
}

Result<size_t> Socket::receive_from(IpAddr & ipaddr, void *data, size_t size)
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

Result<void> Socket::bind(const IpAddr & addr)
{
    return this->bind(&addr.addr(), addr.addr_len());
}

Result<void> Socket::connect(const IpAddr & addr, int timeout_ms)
{
    auto res = this->connect(&addr.addr(), addr.addr_len(), timeout_ms);
    if (res)
    {
        _connect_addr = addr;
        _connect_unix_path.clear();
    }
    return res;
}

Result<int> Socket::accept(IpAddr & ipaddr, int timeout_ms)
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

Result<int> Socket::accept(int timeout_ms)
{
    return this->accept(nullptr, nullptr, timeout_ms);
}

/* ************************************************************************* */
/* Socket multicast */
/* ************************************************************************* */

Result<void> Socket::_multicast_membership(const IpAddr & group, const std::string & iface, int ipv4_opt, int ipv6_opt)
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
            mreq.ipv6mr_interface = if_nametoindex(iface.c_str());
            if (mreq.ipv6mr_interface == 0)
                return std::unexpected(Error(ErrorCode::not_found, "no interface named: {}", iface));
        }
#endif

        return opt_result("multicast membership",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IPV6, ipv6_opt, &mreq, sizeof(mreq)));
    }
    return std::unexpected(Error(ErrorCode::invalid_argument, "multicast group is not an ip address"));
}

Result<void> Socket::join_multicast(const IpAddr & group, const std::string & iface)
{
    return this->_multicast_membership(group, iface, IP_ADD_MEMBERSHIP, IPV6_JOIN_GROUP);
}

Result<void> Socket::leave_multicast(const IpAddr & group, const std::string & iface)
{
    return this->_multicast_membership(group, iface, IP_DROP_MEMBERSHIP, IPV6_LEAVE_GROUP);
}

Result<void> Socket::set_multicast_ttl(int ttl)
{
    if (!this->is_open())
        return std::unexpected(closed_socket_error("set multicast ttl"));

    if (_domain == AF_INET6)
        return opt_result("set multicast ttl",
                          sihd::sys::os::setsockopt(_socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)));

    return opt_result("set multicast ttl",
                      sihd::sys::os::setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)));
}

Result<void> Socket::set_multicast_loop(bool active)
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

Result<void> Socket::bind_unix(std::string_view path)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid unix path: {}", path));

    auto res = this->bind((sockaddr *)&addr, *addr_len);
    if (res)
    {
        // an abstract socket owns no filesystem name: nothing to unlink
        if (path[0] != '\0')
            _unix_bind_path = path;
    }
    return res;
}

Result<void> Socket::connect_unix(std::string_view path)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid unix path: {}", path));

    auto res = this->connect((sockaddr *)&addr, *addr_len);
    if (res)
    {
        _connect_unix_path = path;
        _connect_addr = IpAddr();
    }
    return res;
}

Result<size_t> Socket::send_to_unix(std::string_view path, ArrCharView view)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid unix path: {}", path));

    return this->send_to((sockaddr *)&addr, *addr_len, view);
}

Result<void> Socket::send_all_to_unix(std::string_view path, ArrCharView view)
{
    sockaddr_un addr;
    std::optional<socklen_t> addr_len = unix_path_to_sockaddr(&addr, path);
    if (addr_len == std::nullopt)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid unix path: {}", path));

    return this->send_all_to((sockaddr *)&addr, *addr_len, view);
}

Result<size_t> Socket::receive_from_unix(std::string & path, void *data, size_t size)
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

Result<std::string> Socket::unix_socket_peername(int socket)
{
    sockaddr_un addr_un;
    memset(&addr_un, 0, sizeof(addr_un));
    socklen_t len = sizeof(addr_un);

    auto peer = Socket::get_socket_peername(socket, (sockaddr *)&addr_un, &len);
    SIHD_UNEXPECTED_RETURN(peer);
    return unix_path_from_addr(addr_un, len);
}

/* ************************************************************************* */
/* Socket utilities operations */
/* ************************************************************************* */

} // namespace sihd::net
