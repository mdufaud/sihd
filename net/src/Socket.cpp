#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace sihd::net
{

LOGGER;

Socket::Socket()
{
    this->_init();
}

Socket::~Socket()
{
    this->close();
}

void    Socket::_clear_socket_info()
{
    _domain = -1;
    _type = -1;
    _protocol = -1;
    _socket = -1;
}

void    Socket::_init()
{
    this->_clear_socket_info();
    _rcv_flags = 0;
    _send_flags = 0;
    _verbose = false;
}

bool    Socket::get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot get peer name on a negative socket");
    socklen_t initial = *addr_len;
    int ret = ::getpeername(socket, addr, addr_len);
    if (ret == -1)
        return false;
    return ret == 0 && initial == *addr_len;
}

std::optional<IpAddr>   Socket::get_socket_ip(int socket, bool ipv6)
{
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    socklen_t len = sizeof(addr_in6);
    if (ipv6 && Socket::get_socket_peername(socket, (sockaddr *)&addr_in6, &len))
        return IpAddr(addr_in6, false);
    len = sizeof(addr_in);
    if (Socket::get_socket_peername(socket, (sockaddr *)&addr_in, &len))
        return IpAddr(addr_in, false);
    return std::nullopt;
}

bool    Socket::set_socket_opt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set tcp nodelay on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::setsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::setsockopt(socket, level, optname, (const char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        LOG(error, "Socket: getsockopt error: " << strerror(errno));
    return ret;
}

bool    Socket::get_socket_opt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set tcp nodelay on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::getsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::getsockopt(socket, level, optname, (char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        LOG(error, "Socket: getsockopt error: " << strerror(errno));
    return ret;
}

std::optional<bool>    Socket::is_tcp_nodelay(int socket)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set tcp nodelay on a closed socket");
    int opt;
    socklen_t len = sizeof(opt);
    if (Socket::get_socket_opt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, &len, true))
        return opt != 0;
    return false;
}

bool    Socket::set_tcp_nodelay(int socket, bool active)
{
    if (socket < 0)
        throw std::runtime_error("Socket: cannot set tcp nodelay on a closed socket");
    int opt = (active ? 1 : 0);
    return Socket::set_socket_opt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt), true);
}

bool    Socket::open(const std::string & domain, const std::string & type, const std::string & protocol)
{
    int sockdomain = Ip::domain(domain);
    int socktype = Ip::socktype(type);
    int sockprotocol = Ip::protocol(protocol);
    if (sockdomain == -1)
        LOG(error, "Socket: domain unknown: " << domain);
    if (socktype == -1)
        LOG(error, "Socket: socket type unknown: " << type);
    if (sockprotocol == -1)
        LOG(error, "Socket: protocol unknown: " << protocol);
    if (sockdomain >= 0 && socktype >= 0 && sockprotocol >= 0)
        return this->open(sockdomain, socktype, sockprotocol);
    return false;
}

bool    Socket::open(int domain, int type, int protocol)
{
    if (this->is_open())
        return false;
    _socket = ::socket(domain, type, protocol);
    if (_socket < 0)
        LOG(error, "Socket: " << strerror(errno));
    _domain = domain;
    _type = type;
    _protocol = protocol;
    return _socket >= 0;
}

bool    Socket::close()
{
    bool ret = true;
    if (_socket >= 0)
    {
        ret = ::close(_socket) == 0;
        if (ret == false)
            LOG(error, "Socket: close error: " << strerror(errno));
        this->_clear_socket_info();
    }
    return ret;
}

bool    Socket::shutdown()
{
    bool ret = true;
    if (_socket >= 0)
    {
        ret = ::shutdown(_socket, SHUT_RDWR) == 0;
        if (ret == false)
            LOG(error, "Socket: shutdown error: " << strerror(errno));
    }
    return ret;
}

int     Socket::accept(sockaddr *addr, socklen_t *addr_len)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot accept on a closed socket");
    int sock = ::accept(_socket, addr, addr_len);
    if (sock < 0)
        LOG(error, "Socket: accept error: " << strerror(errno));
    return sock;
}

Socket::AcceptInfo  Socket::accept_info(bool ipv6)
{
    sockaddr *addr;
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    socklen_t len;
    if (ipv6)
    {
        addr = (sockaddr *)&addr_in6;
        len = sizeof(addr_in6);
    }
    else
    {
        addr = (sockaddr *)&addr_in;
        len = sizeof(addr_in);
    }
    int sock = this->accept(addr, &len);
    AcceptInfo info;
    info.socket = sock;
    if (sock >= 0)
    {
        if (ipv6)
        {
            info.port = ntohs(addr_in6.sin6_port);
            info.addr = IpAddr::ip_to_string(addr_in6);
        }
        else
        {
            info.port = ntohs(addr_in.sin_port);
            info.addr = IpAddr::ip_to_string(addr_in);
        }
    }
    return info;
}

bool    Socket::listen(uint16_t queue_size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot listen on a closed socket");
    if (::listen(_socket, queue_size) == -1)
    {
        LOG(error, "Socket: listen error: " << strerror(errno));
        return false;
    }
    return true;
}

bool    Socket::bind(sockaddr *addr, socklen_t addr_len)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot bind on a closed socket");
    if (::bind(_socket, addr, addr_len) == -1)
    {
        LOG(error, "Socket: bind error: " << strerror(errno));
        return false;
    }
    return true;
}

bool    Socket::connect(sockaddr *addr, socklen_t addr_len)
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
                LOG(error, "Socket: connect error: " << strerror(errno));
            return false;
        }
    }
    return true;
}

ssize_t     Socket::send_to(const IpAddr & addr, void *data, size_t size)
{
    sockaddr_in addr_in;

    if (addr.get_sockaddr_in(&addr_in, _type, _protocol) == false
        && addr.get_first_sockaddr_in(&addr_in) == false)
        return false;
    return this->send_to((sockaddr *)&addr_in, sizeof(sockaddr_in), data, size);
}

ssize_t     Socket::receive_from(const IpAddr & addr, void *data, size_t size)
{
    sockaddr_in addr_in;
    
    if (addr.get_sockaddr_in(&addr_in, _type, _protocol) == false
        && addr.get_first_sockaddr_in(&addr_in) == false)
        return false;
    return this->receive_from((sockaddr *)&addr_in, sizeof(sockaddr_in), data, size);
}

bool    Socket::bind(const IpAddr & addr)
{
    sockaddr_in addr_in;
    
    if (addr.get_sockaddr_in(&addr_in, _type, _protocol) == false
        && addr.get_first_sockaddr_in(&addr_in) == false)
        return false;
    return this->bind((sockaddr *)&addr_in, sizeof(sockaddr_in));
}

bool    Socket::connect(const IpAddr & addr)
{
    sockaddr_in addr_in;
    
    if (addr.get_sockaddr_in(&addr_in, _type, _protocol) == false
        && addr.get_first_sockaddr_in(&addr_in) == false)
        return false;
    return this->connect((sockaddr *)&addr_in, sizeof(sockaddr_in));
}

bool    Socket::bind(const std::string & ip, int port)
{
    IpAddr::IpSockAddr ipsockaddr;

    if (IpAddr::fill_sockaddr(ip, &ipsockaddr, port) == false)
    {
        LOG(error, "Socket: could not convert ip '" << ip << "' to sockaddr");
        return -1;
    }
    return this->bind(ipsockaddr.addr, ipsockaddr.addr_len);
}

bool    Socket::connect(const std::string & ip, int port)
{
    IpAddr::IpSockAddr ipsockaddr;

    if (IpAddr::fill_sockaddr(ip, &ipsockaddr, port) == false)
    {
        LOG(error, "Socket: could not convert ip '" << ip << "' to sockaddr");
        return -1;
    }
    return this->connect(ipsockaddr.addr, ipsockaddr.addr_len);
}

ssize_t     Socket::send_to(const std::string & ip, void *data, size_t size)
{
    IpAddr::IpSockAddr ipsockaddr;

    if (IpAddr::fill_sockaddr(ip, &ipsockaddr) == false)
    {
        LOG(error, "Socket: could not convert ip '" << ip << "' to sockaddr");
        return -1;
    }
    return this->send_to(ipsockaddr.addr, ipsockaddr.addr_len, data, size);
}

ssize_t     Socket::receive_from(const std::string & ip, void *data, size_t size)
{
    IpAddr::IpSockAddr ipsockaddr;

    if (IpAddr::fill_sockaddr(ip, &ipsockaddr) == false)
    {
        LOG(error, "Socket: could not convert ip '" << ip << "' to sockaddr");
        return -1;
    }
    return this->receive_from(ipsockaddr.addr, ipsockaddr.addr_len, data, size);
}

ssize_t     Socket::send(void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot send on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t sent = ::send(_socket, data, size, _send_flags);
#else
    ssize_t sent = ::send(_socket, (const char *)data, size, _send_flags);
#endif
    if (sent < 0 && _verbose)
        LOG(warning, "Socket send error: " << strerror(errno));
    return sent;
}

ssize_t     Socket::receive(void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot receive on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t rcv = ::recv(_socket, data, size, _rcv_flags);
#else
    ssize_t rcv = ::recv(_socket, (char *)data, size, _rcv_flags);
#endif
    if (rcv < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        LOG(error, "Socket receive error: " << strerror(errno));
    return rcv;
}

ssize_t     Socket::send_to(sockaddr *addr, socklen_t addr_len, void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot send_to on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t sent = ::sendto(_socket, data, size, _send_flags, addr, addr_len);
#else
    ssize_t sent = ::sendto(_socket, (const char *)data, size, _send_flags, addr, addr_len);
#endif
    if (sent < 0 && _verbose)
        LOG(warning, "Socket send_to error: " << strerror(errno));
    return sent;
}

ssize_t     Socket::receive_from(sockaddr *addr, socklen_t addr_len, void *data, size_t size)
{
    if (this->is_open() == false)
        throw std::runtime_error("Socket: cannot receive_from on a closed socket");
#if !defined(__SIHD_WINDOWS__)
    ssize_t rcv = ::recvfrom(_socket, data, size, _send_flags, addr, &addr_len);
#else
    ssize_t rcv = ::recvfrom(_socket, (char *)data, size, _send_flags, addr, &addr_len);
#endif
    if (rcv < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        LOG(error, "Socket receive_from error: " << strerror(errno));
    return rcv;
}

#if !defined(__SIHD_WINDOWS__)

bool        Socket::bind_unix(const std::string & path)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());
    return this->bind((sockaddr *)&addr, SUN_LEN(&addr));
}

bool        Socket::connect_unix(const std::string & path)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());  
    return this->connect((sockaddr *)&addr, SUN_LEN(&addr));
}

ssize_t     Socket::send_to_unix(const std::string & path, void *data, size_t size)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());
    return this->send_to((sockaddr *)&addr, SUN_LEN(&addr), data, size);
}

ssize_t     Socket::receive_from_unix(const std::string & path, void *data, size_t size)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());
    return this->receive_from((sockaddr *)&addr, SUN_LEN(&addr), data, size);
}

std::string   Socket::get_unix_socket_peername(int socket)
{
    sockaddr_un addr_un;
    socklen_t len = sizeof(addr_un);
    if (Socket::get_socket_peername(socket, (sockaddr *)&addr_un, &len))
        return addr_un.sun_path;
    return "";
}

#endif

}