#include <unistd.h>

#include <cerrno>
#include <cstring>

#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <fcntl.h>       // fcntl
# include <net/if.h>      // IFNAMSIZ
# include <netinet/tcp.h> // tcp nodelay
# include <sys/un.h>      // unix sockets
#else
# include <afunix.h>   // AF_UNIX
# include <ws2tcpip.h> // IP_TTL SUN_LEN
// missing mingw getsockopt action
# ifndef SO_BSP_STATE
#  define SO_BSP_STATE 0x1009
# endif

/* Evaluate to actual length of the `sockaddr_un' structure.  */
# define SUN_LEN(ptr) (offsetof(struct sockaddr_un, sun_path) + strlen((ptr)->sun_path))

#endif

namespace sihd::net
{

namespace
{

ssize_t _adapt_array_size(sihd::util::IArray & arr, ssize_t sent)
{
    if (sent >= 0 && (size_t)sent <= arr.byte_capacity() && (size_t)sent != arr.byte_size())
        arr.byte_resize((size_t)sent);
    return sent;
}

} // namespace

SIHD_LOGGER;

Socket::Socket()
{
    this->_clear_socket_info();
    _rcv_flags = 0;
    _send_flags = 0;
    _verbose = false;
}

Socket::Socket(int domain, int socket_type, int protocol): Socket()
{
    this->open(domain, socket_type, protocol);
}

Socket::Socket(std::string_view domain, std::string_view socket_type, std::string_view protocol): Socket()
{
    this->open(domain, socket_type, protocol);
}

Socket::Socket(int socket, bool get_infos): Socket()
{
    if (socket >= 0)
    {
        _socket = socket;
        if (get_infos && this->get_infos() == false)
            this->_clear_socket_info();
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

Socket::Socket(Socket && other)
{
    *this = std::move(other);
}

Socket & Socket::operator=(Socket && other)
{
    _socket = other._socket;
    _domain = other._domain;
    _type = other._type;
    _protocol = other._protocol;
    _verbose = other._verbose;
    _send_flags = other._send_flags;
    _rcv_flags = other._rcv_flags;

    other._clear_socket_info();
    return *this;
}

Socket::~Socket()
{
    this->shutdown();
    this->close();
}

void Socket::_clear_socket_info()
{
    _domain = -1;
    _type = -1;
    _protocol = -1;
    _socket = -1;
}

/* ************************************************************************* */
/* Static class utilities */
/* ************************************************************************* */

bool Socket::set_socket_ttl(int socket, int ttl, bool ipv6)
{
    return sihd::util::os::setsockopt(socket, ipv6 ? IPPROTO_IPV6 : IPPROTO_IP, IP_TTL, &ttl, sizeof(int));
}

bool Socket::set_socket_reuseaddr(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return sihd::util::os::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
}

bool Socket::set_socket_broadcast(int socket, bool active)
{
    int opt = active ? 1 : 0;
    return sihd::util::os::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(int));
}

bool Socket::set_socket_tcp_nodelay(int socket, bool active)
{
    int opt = (active ? 1 : 0);
    return sihd::util::os::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt), true);
}

bool Socket::is_socket_tcp_nodelay(int socket)
{
    int opt;
    socklen_t len = sizeof(opt);
    return sihd::util::os::getsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, &len, true) && opt != 0;
}

bool Socket::is_socket_broadcast(int socket)
{
    int res;
    socklen_t length = sizeof(int);
    return sihd::util::os::getsockopt(socket, SOL_SOCKET, SO_BROADCAST, &res, &length, true) && res != 0;
}

bool Socket::bind_socket_to_device(int socket, std::string_view name)
{
#if !defined(__SIHD_WINDOWS__)
    char device_name[IFNAMSIZ];

    strncpy(device_name, name.data(), std::min(name.size(), (size_t)IFNAMSIZ));
    return sihd::util::os::setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, device_name, sizeof(device_name), true);
#else
    (void)socket;
    (void)name;
    return false;
#endif
}

bool Socket::get_socket_infos(int socket, int *domain, int *type, int *protocol)
{
#if !defined(__SIHD_WINDOWS__)
    socklen_t length = sizeof(int);
    bool found = sihd::util::os::getsockopt(socket, SOL_SOCKET, SO_DOMAIN, domain, &length);
    length = sizeof(int);
    found = found && sihd::util::os::getsockopt(socket, SOL_SOCKET, SO_TYPE, type, &length);
    length = sizeof(int);
    found = found && sihd::util::os::getsockopt(socket, SOL_SOCKET, SO_PROTOCOL, protocol, &length);
    return found;
#else
    CSADDR_INFO addrinfo;
    socklen_t length = sizeof(addrinfo);
    bool found = sihd::util::os::getsockopt(socket, SOL_SOCKET, SO_BSP_STATE, &addrinfo, &length);
    if (found)
    {
        *protocol = addrinfo.iProtocol;
        *type = addrinfo.iSocketType;
        *domain = AF_INET;
    }
    return found;
#endif
}

bool Socket::get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot get peer name on a negative socket");
    socklen_t initial = *addr_len;
    int ret = ::getpeername(socket, addr, addr_len);
    if (ret == -1)
        return false;
    return ret == 0 && initial == *addr_len;
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
    auto opt_ip = this->peeraddr(true);
    return opt_ip ? opt_ip.value().port() : -1;
}

/* ************************************************************************* */
/* Socket open/close */
/* ************************************************************************* */

bool Socket::get_infos()
{
    return _socket >= 0 && Socket::get_socket_infos(_socket, &_domain, &_type, &_protocol);
}

bool Socket::set_tcp_nodelay(bool active) const
{
    return Socket::set_socket_tcp_nodelay(_socket, active);
}
bool Socket::set_blocking(bool active) const
{
    return Socket::set_socket_blocking(_socket, active);
}
bool Socket::set_reuseaddr(bool active) const
{
    return Socket::set_socket_reuseaddr(_socket, active);
}
bool Socket::set_broadcast(bool active) const
{
    return Socket::set_socket_broadcast(_socket, active);
}
bool Socket::bind_to_device(std::string_view name) const
{
    return Socket::bind_socket_to_device(_socket, name);
}
bool Socket::is_tcp_nodelay() const
{
    return Socket::is_socket_tcp_nodelay(_socket);
}
bool Socket::is_blocking() const
{
    return Socket::is_socket_blocking(_socket);
}
bool Socket::is_broadcast() const
{
    return Socket::is_socket_broadcast(_socket);
}
bool Socket::set_ttl(int ttl) const
{
    return Socket::set_socket_ttl(_socket, ttl, this->is_ipv6());
}

bool Socket::open(std::string_view domain, std::string_view type, std::string_view protocol)
{
    int sockdomain = ip::domain(domain);
    int socktype = ip::socktype(type);
    int sockprotocol = ip::protocol(protocol);
    if (sockdomain == -1)
        SIHD_LOG(error, "Socket: domain unknown: {}", domain);
    if (socktype == -1)
        SIHD_LOG(error, "Socket: socket type unknown: {}", type);
    if (sockprotocol == -1)
        SIHD_LOG(error, "Socket: protocol unknown: {}", protocol);
    if (sockdomain >= 0 && socktype >= 0 && sockprotocol >= 0)
        return this->open(sockdomain, socktype, sockprotocol);
    return false;
}

bool Socket::open(int domain, int type, int protocol)
{
    if (this->is_open())
        return false;
    _socket = ::socket(domain, type, protocol);
    if (_socket < 0)
        SIHD_LOG(error, "Socket: {}", strerror(errno));
    _domain = domain;
    _type = type;
    _protocol = protocol;
    return _socket >= 0;
}

bool Socket::close()
{
    bool ret = true;
    if (_socket >= 0)
    {
        ret = ::close(_socket) == 0;
        if (ret == false)
            SIHD_LOG(error, "Socket: close error: {}", strerror(errno));
        this->_clear_socket_info();
    }
    return ret;
}

bool Socket::shutdown()
{
    bool ret = true;
    if (_socket >= 0)
    {
        ret = ::shutdown(_socket, SHUT_RDWR) == 0;
        // no error message if socket was not connected
        if (ret == false && errno != ENOTCONN)
            SIHD_LOG(error, "Socket: shutdown error: {}", strerror(errno));
    }
    return ret;
}

/* ************************************************************************* */
/* Socket sockaddr operations */
/* ************************************************************************* */

int Socket::accept(sockaddr *addr, socklen_t *addr_len)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot accept on a closed socket");
    int sock = ::accept(_socket, addr, addr_len);
    if (sock < 0)
        SIHD_LOG(error, "Socket: accept error: {}", strerror(errno));
    return sock;
}

bool Socket::listen(uint16_t queue_size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot listen on a closed socket");
    if (::listen(_socket, queue_size) == -1)
    {
        SIHD_LOG(error, "Socket: listen error: {}", strerror(errno));
        return false;
    }
    return true;
}

bool Socket::bind(const sockaddr *addr, socklen_t addr_len)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot bind on a closed socket");
    if (::bind(_socket, addr, addr_len) == -1)
    {
        SIHD_LOG(error, "Socket: bind error: {}", strerror(errno));
        return false;
    }
    return true;
}

bool Socket::connect(const sockaddr *addr, socklen_t addr_len)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot connect on a closed socket");
    if (::connect(_socket, addr, addr_len) == -1)
    {
        // EISCONN: already connected (not an error)
        if (errno != EISCONN)
        {
            // EINPROGRESS: socket is non blocking and connection is establishing
            // EALREADY: socket is non blocking and a connect is already pending
            // no error print but not connected
            if (errno != EINPROGRESS && errno != EALREADY)
                SIHD_LOG(error, "Socket: connect error: {}", strerror(errno));
            return false;
        }
    }
    return true;
}

ssize_t Socket::send(sihd::util::ArrCharView view)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot send on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t sent = ::send(_socket, view.data(), view.size(), _send_flags);
#else
    ssize_t sent = ::send(_socket, (const char *)view.data(), view.size(), _send_flags);
#endif
    if (sent < 0 && _verbose)
        SIHD_LOG(warning, "Socket send error: {}", strerror(errno));
    return sent;
}

bool Socket::send_all(sihd::util::ArrCharView view)
{
    ssize_t ret;
    size_t sent = 0;

    while (sent < view.size())
    {
        ret = this->send({view.data() + sent, view.size() - sent});
        if (ret <= 0)
            return false;
        sent += ret;
    }
    return sent == view.size();
}

ssize_t Socket::receive(void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot receive on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t rcv = ::recv(_socket, data, size, _rcv_flags);
#else
    ssize_t rcv = ::recv(_socket, (char *)data, size, _rcv_flags);
#endif
    if (rcv < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        SIHD_LOG(error, "Socket receive error: {}", strerror(errno));
    return rcv;
}

ssize_t Socket::receive(sihd::util::IArray & arr)
{
    return _adapt_array_size(arr, this->receive(arr.buf(), arr.byte_capacity()));
}

ssize_t Socket::send_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot send_to on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t sent = ::sendto(_socket, view.data(), view.size(), _send_flags, addr, addr_len);
#else
    ssize_t sent = ::sendto(_socket, (const char *)view.data(), view.size(), _send_flags, addr, addr_len);
#endif
    if (sent < 0 && _verbose)
        SIHD_LOG(warning, "Socket send_to error: {}", strerror(errno));
    return sent;
}

bool Socket::send_all_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view)
{
    ssize_t ret;
    size_t sent = 0;

    while (sent < view.size())
    {
        ret = this->send_to(addr, addr_len, {view.data() + sent, view.size() - sent});
        if (ret <= 0)
            return ret;
        sent += ret;
    }
    return sent == view.size();
}

ssize_t Socket::receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot receive_from on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t rcv = ::recvfrom(_socket, data, size, _send_flags, addr, addr_len);
#else
    ssize_t rcv = ::recvfrom(_socket, (char *)data, size, _send_flags, addr, addr_len);
#endif
    if (rcv < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        SIHD_LOG(error, "Socket receive_from error: {}", strerror(errno));
    return rcv;
}

ssize_t Socket::receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr)
{
    return _adapt_array_size(arr, this->receive_from(addr, addr_len, arr.buf(), arr.byte_capacity()));
}

ssize_t Socket::receive_from(IpAddr & addr, sihd::util::IArray & arr)
{
    return _adapt_array_size(arr, this->receive_from(addr, arr.buf(), arr.byte_capacity()));
}

ssize_t Socket::receive_from_unix(std::string & path, sihd::util::IArray & arr)
{
    return _adapt_array_size(arr, this->receive_from_unix(path, arr.buf(), arr.byte_capacity()));
}

bool Socket::is_unix() const
{
#if !defined(__SIHD_WINDOWS__)
    return _domain == AF_UNIX;
#else
    return false;
#endif
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

ssize_t Socket::send_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return this->send_to(&addr.addr(), addr.addr_len(), view);
}

bool Socket::send_all_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return this->send_all_to(&addr.addr(), addr.addr_len(), view);
}

ssize_t Socket::receive_from(IpAddr & ipaddr, void *data, size_t size)
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
    int sock = this->receive_from(addr, &len, data, size);
    if (sock >= 0)
        ipaddr = IpAddr(*addr, len);
    return sock;
}

bool Socket::bind(const IpAddr & addr)
{
    return this->bind(&addr.addr(), addr.addr_len());
}

bool Socket::connect(const IpAddr & addr)
{
    return this->connect(&addr.addr(), addr.addr_len());
}

int Socket::accept(IpAddr & ipaddr)
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
    int sock = this->accept(addr, &len);
    if (sock >= 0)
        ipaddr = IpAddr(*addr, len);
    return sock;
}

int Socket::accept()
{
    return this->accept(nullptr, nullptr);
}

/* ************************************************************************* */
/* Socket UNIX operations */
/* ************************************************************************* */

bool Socket::bind_unix(std::string_view path)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.data(), std::min(path.size(), sizeof(addr.sun_path)));
    return this->bind((sockaddr *)&addr, SUN_LEN(&addr));
}

bool Socket::connect_unix(std::string_view path)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.data(), std::min(path.size(), sizeof(addr.sun_path)));
    return this->connect((sockaddr *)&addr, SUN_LEN(&addr));
}

ssize_t Socket::send_to_unix(std::string_view path, sihd::util::ArrCharView view)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.data(), std::min(path.size(), sizeof(addr.sun_path)));
    return this->send_to((sockaddr *)&addr, SUN_LEN(&addr), view);
}

bool Socket::send_all_to_unix(std::string_view path, sihd::util::ArrCharView view)
{
    ssize_t ret;
    size_t sent = 0;
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.data(), std::min(path.size(), sizeof(addr.sun_path)));
    size_t sun_len = SUN_LEN(&addr);

    while (sent < view.size())
    {
        ret = this->send_to((sockaddr *)&addr, sun_len, {view.data() + sent, view.size() - sent});
        if (ret <= 0)
            return ret;
        sent += ret;
    }
    return sent;
}

ssize_t Socket::receive_from_unix(std::string & path, void *data, size_t size)
{
    sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = SUN_LEN(&addr);
    ssize_t ret = this->receive_from((sockaddr *)&addr, &addr_len, data, size);
    if (ret > 0)
        path = addr.sun_path;
    return ret;
}

std::string Socket::unix_socket_peername(int socket)
{
    sockaddr_un addr_un;
    socklen_t len = sizeof(addr_un);
    if (Socket::get_socket_peername(socket, (sockaddr *)&addr_un, &len))
        return addr_un.sun_path;
    return "";
}

/* ************************************************************************* */
/* Socket (unix) utilities operations */
/* ************************************************************************* */

#if !defined(__SIHD_WINDOWS__)

bool Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set blocking on a closed socket");
    int opts = ::fcntl(socket, F_GETFL);
    if (opts < 0)
    {
        SIHD_LOG(error, "Socket: could not get fcntl: {}", strerror(errno));
        return false;
    }
    if (active)
        opts |= O_NONBLOCK;
    else
        opts &= ~O_NONBLOCK;
    opts = ::fcntl(socket, F_SETFL, opts);
    if (opts < 0)
        SIHD_LOG(error, "Socket: could not set fcntl options: {}", strerror(errno));
    return opts >= 0;
}

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set blocking on a closed socket");
    int opts = ::fcntl(socket, F_GETFL);
    if (opts < 0)
    {
        SIHD_LOG(error, "Socket: could not get fcntl: {}", strerror(errno));
        return false;
    }
    return opts & O_NONBLOCK;
}

#else

/* ************************************************************************* */
/* Socket windows operations */
/* ************************************************************************* */

bool Socket::is_socket_blocking(int socket)
{
    if (socket < 0)
        throw std::runtime_error("Socket: check blocking on a closed socket");
    /// @note windows sockets are created in blocking mode by default
    // currently on windows, there is no easy way to obtain the socket's current blocking mode since WSAIsBlocking was
    // deprecated
    unsigned long mode = 1;
    bool set_blocking = util::os::ioctl(socket, FIONBIO, &mode);
    if (set_blocking)
    {
        // put back non blocking
        mode = 0;
        return util::os::ioctl(socket, FIONBIO, &mode, true);
    }
    return set_blocking == false;
}

bool Socket::set_socket_blocking(int socket, bool active)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set blocking on a closed socket");
    unsigned long mode = active ? 0 : 1;
    if (sihd::util::os::ioctl(socket, FIONBIO, &mode) != 0)
    {
        SIHD_LOG(error, "Socket: could not set ioctl: {}", strerror(errno));
        return false;
    }
    return true;
}

#endif

} // namespace sihd::net
