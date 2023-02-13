#ifndef __SIHD_NET_SOCKET_HPP__
# define __SIHD_NET_SOCKET_HPP__

# include <sihd/net/Ip.hpp>
# include <sihd/net/IpAddr.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/ArrayView.hpp>
# include <sihd/util/os.hpp>

# if !defined(__SIHD_WINDOWS__)
#  include <netinet/tcp.h>
#  include <fcntl.h>
# else
#  define SHUT_RD SD_RECEIVE
#  define SHUT_WR SD_SEND
#  define SHUT_RDWR SD_BOTH
# endif

namespace sihd::net
{

class Socket
{
    public:
        Socket();
        // open socket
        Socket(int domain, int socket_type, int protocol);
        Socket(std::string_view domain, std::string_view socket_type, std::string_view protocol);
        // from existing socket - get_infos perform up to 3 getsockopt
        Socket(int socket, bool get_infos = true);
        Socket(int socket, int domain, int socket_type, int protocol);
        Socket(int socket, std::string_view domain, std::string_view socket_type, std::string_view protocol);
        Socket(Socket && other);
        virtual ~Socket();

        // don't like hidden behavior so i prefer deleting copy operators
        Socket(const Socket & other) = delete;
        Socket & operator=(const Socket & other) = delete;

        Socket & operator=(Socket && other);

        operator int() const { return _socket; }
        operator bool() const { return this->is_open(); }

        // Class utilities for socket manipulation //

        // returns true if getpeername worked and the provided addr_len is still the same
        static bool get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len);

        // return an IpAdress from socket using get_socket_peername; if ipv6 is true, checks for an ipv6 addr first
        static std::optional<IpAddr> socket_ip(int socket, bool ipv6 = false);
        static bool get_socket_infos(int socket, int *domain, int *type, int *protocol);

        static bool set_socket_tcp_nodelay(int socket, bool active);
        static bool set_socket_blocking(int socket, bool active);
        static bool set_socket_reuseaddr(int socket, bool active);
        static bool set_socket_broadcast(int socket, bool active);
        static bool bind_socket_to_device(int socket, std::string_view name);
        static bool is_socket_tcp_nodelay(int socket);
        static bool is_socket_blocking(int socket);
        static bool is_socket_broadcast(int socket);
        static bool set_socket_ttl(int socket, int ttl, bool ipv6 = false);

        // Operations on internal socket //

        bool get_infos();

        bool set_tcp_nodelay(bool active) const { return Socket::set_socket_tcp_nodelay(_socket, active); }
        bool set_blocking(bool active) const { return Socket::set_socket_blocking(_socket, active); }
        bool set_reuseaddr(bool active) const { return Socket::set_socket_reuseaddr(_socket, active); }
        bool set_broadcast(bool active) const { return Socket::set_socket_broadcast(_socket, active); }
        bool bind_to_device(std::string_view name) const { return Socket::bind_socket_to_device(_socket, name); }
        bool is_tcp_nodelay() const { return Socket::is_socket_tcp_nodelay(_socket); }
        bool is_blocking() const { return Socket::is_socket_blocking(_socket); }
        bool is_broadcast() const { return Socket::is_socket_broadcast(_socket); }
        bool set_ttl(int ttl) const { return Socket::set_socket_ttl(_socket, ttl, this->is_ipv6()); }

        bool open(std::string_view domain, std::string_view type, std::string_view protocol);
        bool open(int domain, int socket_type, int protocol);
        bool close();
        bool shutdown();
        bool is_open() const { return _socket >= 0; }

        ssize_t send(sihd::util::ArrViewChar view);
        bool send_all(sihd::util::ArrViewChar view);

        ssize_t receive(void *data, size_t size);
        ssize_t receive(sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive(arr.buf(), arr.byte_capacity())); }

        bool listen(uint16_t queue_size);
        // fill addr and addr_len based on incoming connections
        int accept(sockaddr *addr, socklen_t *addr_len);
        int accept() { return this->accept(nullptr, nullptr); }
        int accept(IpAddr & ipaddr);

        // Utilities for internal socket //

        std::optional<IpAddr> peeraddr(bool ipv6 = false) const { return Socket::socket_ip(_socket, ipv6); }
        int local_port() const
        {
            auto opt_ip = this->peeraddr(true);
            return opt_ip ? opt_ip.value().port() : -1;
        }

        // Operations on IP adresses //

        // sockaddr
        bool bind(const sockaddr *addr, socklen_t addr_len);
        bool connect(const sockaddr *addr, socklen_t addr_len);
        ssize_t send_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrViewChar view);
        bool send_all_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrViewChar view);
        ssize_t receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size);
        ssize_t receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive_from(addr, addr_len, arr.buf(), arr.byte_capacity())); }

        /*
            sihd::net::IpAddr
        */
        bool bind(const IpAddr & addr);
        bool connect(const IpAddr & addr);
        // calls send_to_ip  or first IPV4 ip
        ssize_t send_to(const IpAddr & addr, sihd::util::ArrViewChar view);
        bool send_all_to(const IpAddr & addr, sihd::util::ArrViewChar view);
        ssize_t receive_from(IpAddr & addr, void *data, size_t size);
        ssize_t receive_from(IpAddr & addr, sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive_from(addr, arr.buf(), arr.byte_capacity())); }

        // Operations on unix sockets //

        static std::string unix_socket_peername(int socket);
        bool bind_unix(std::string_view path);
        bool connect_unix(std::string_view path);
        ssize_t send_to_unix(std::string_view path, sihd::util::ArrViewChar view);
        bool send_all_to_unix(std::string_view path, sihd::util::ArrViewChar view);
        ssize_t receive_from_unix(std::string & path, void *data, size_t size);
        ssize_t receive_from_unix(std::string & path, sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive_from_unix(path, arr.buf(), arr.byte_capacity())); }

        // Configuration //

        void set_verbose(bool active) { _verbose = active; }
        void set_send_flags(int flags) { _send_flags = flags; }
        void set_receive_flags(int flags) { _rcv_flags = flags; }

        // Getters //

        int domain() const { return _domain; }
        int type() const { return _type; }
        int protocol() const { return _protocol; }
        int socket() const { return _socket; }

        bool is_ipv6() const { return _domain == AF_INET6; }

    protected:

    private:
        void _clear_socket_info();
        // returns sent value
        static ssize_t _adapt_array_size(sihd::util::IArray & array, ssize_t sent);

        int _domain;
        int _type;
        int _protocol;
        int _socket;

        bool _verbose;
        int _send_flags;
        int _rcv_flags;
};

}

#endif