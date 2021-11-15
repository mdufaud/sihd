#ifndef __SIHD_NET_SOCKET_HPP__
# define __SIHD_NET_SOCKET_HPP__

# include <sihd/net/Ip.hpp>
# include <sihd/net/IpAddr.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/OS.hpp>

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
        // opens
        Socket(int domain, int socket_type, int protocol);
        Socket(const std::string & domain, const std::string & socket_type, const std::string & protocol);
        // from existing socket
        Socket(int socket);
        Socket(int socket, int domain, int socket_type, int protocol);
        Socket(int socket, const std::string & domain, const std::string & socket_type, const std::string & protocol);

        virtual ~Socket();

        // Class utilities for socket manipulation //

        // returns true if getpeername worked and the provided addr_len is still the same
        static bool get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len);

        // return an IpAdress from socket using get_socket_peername; if ipv6 is true, checks for an ipv6 addr first
        static std::optional<IpAddr> get_socket_ip(int socket, bool ipv6 = false);
        static bool get_socket_infos(int socket, int *domain, int *type, int *protocol);

        static bool set_socket_blocking(int socket, bool active);
        static bool set_socket_reuseaddr(int socket, bool active);
        static bool set_socket_broadcast(int socket, bool active);
        static bool set_socket_tcp_nodelay(int socket, bool active);
        static bool is_socket_blocking(int socket);
        static bool is_socket_broadcast(int socket);
        static bool is_socket_tcp_nodelay(int socket);

        // Operations on internal socket //

        bool open(const std::string & domain, const std::string & type, const std::string & protocol);
        bool open(int domain, int socket_type, int protocol);
        bool close();
        bool shutdown();
        bool is_open() const { return _socket >= 0; }

        ssize_t send(const void *data, size_t size);
        bool send_all(const void *data, size_t size);
        ssize_t send(const sihd::util::IArray & arr) { return this->send(arr.cbuf(), arr.byte_size()); }
        bool send_all(const sihd::util::IArray & arr) { return this->send_all(arr.cbuf(), arr.byte_size()); }

        ssize_t receive(void *data, size_t size);
        ssize_t receive(sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive(arr.buf(), arr.byte_capacity())); }

        bool listen(uint16_t queue_size);
        // fill addr and addr_len based on incoming connections
        int accept(sockaddr *addr, socklen_t *addr_len);
        int accept() { return this->accept(nullptr, nullptr); }
        int accept(IpAddr & ipaddr);

        // Utilities for internal socket //

        std::optional<IpAddr> get_peername(bool ipv6 = false) const { return Socket::get_socket_ip(_socket, ipv6); }
        int get_local_port() const
        {
            auto opt_ip = this->get_peername(true);
            return opt_ip ? opt_ip.value().port() : -1;
        }
        bool set_tcp_nodelay(bool active) const { return Socket::set_socket_tcp_nodelay(_socket, active); }
        bool set_blocking(bool active) const { return Socket::set_socket_blocking(_socket, active); }
        bool set_reuseaddr(bool active) const { return Socket::set_socket_reuseaddr(_socket, active); }
        bool set_broadcast(bool active) const { return Socket::set_socket_broadcast(_socket, active); }

        bool is_tcp_nodelay() const { return Socket::is_socket_tcp_nodelay(_socket); }
        bool is_blocking() const { return Socket::is_socket_blocking(_socket); }
        bool is_broadcast() const { return Socket::is_socket_broadcast(_socket); }

        // Operations on IP adresses //

        // sockaddr
        bool bind(const sockaddr *addr, socklen_t addr_len);
        bool connect(const sockaddr *addr, socklen_t addr_len);
        ssize_t send_to(const sockaddr *addr, socklen_t addr_len, const void *data, size_t size);
        bool send_all_to(const sockaddr *addr, socklen_t addr_len, const void *data, size_t size);
        ssize_t receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size);
        // sihd::util::IArray
        ssize_t send_to(const sockaddr *addr, socklen_t addr_len, const sihd::util::IArray & arr)
            { return this->send_to(addr, addr_len, arr.cbuf(), arr.byte_size()); }
        bool send_all_to(const sockaddr *addr, socklen_t addr_len, const sihd::util::IArray & arr)
            { return this->send_all_to(addr, addr_len, arr.cbuf(), arr.byte_size()); }
        ssize_t receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive_from(addr, addr_len, arr.buf(), arr.byte_capacity())); }

        // sockaddr_in6
        bool bind(const sockaddr_in6 & addr)
            { return this->bind((sockaddr *)&addr, sizeof(sockaddr_in6)); }
        bool connect(const sockaddr_in6 & addr)
            { return this->connect((sockaddr *)&addr, sizeof(sockaddr_in6)); }
        ssize_t send_to(const sockaddr_in6 & addr, const void *data, size_t size)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in6), data, size); }
        bool send_all_to(const sockaddr_in6 & addr, const void *data, size_t size)
            { return this->send_all_to((sockaddr *)&addr, sizeof(sockaddr_in6), data, size); }
        // sihd::util::IArray
        ssize_t send_to(const sockaddr_in6 & addr, const sihd::util::IArray & arr)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in6), arr); }
        bool send_all_to(const sockaddr_in6 & addr, const sihd::util::IArray & arr)
            { return this->send_all_to((sockaddr *)&addr, sizeof(sockaddr_in6), arr); }

        // sockaddr_in
        bool bind(const sockaddr_in & addr)
            { return this->bind((sockaddr *)&addr, sizeof(sockaddr_in)); }
        bool connect(const sockaddr_in & addr)
            { return this->connect((sockaddr *)&addr, sizeof(sockaddr_in)); }
        ssize_t send_to(const sockaddr_in & addr, const void *data, size_t size)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in), data, size); }
        bool send_all_to(const sockaddr_in & addr, const void *data, size_t size)
            { return this->send_all_to((sockaddr *)&addr, sizeof(sockaddr_in), data, size); }
        // sihd::util::IArray
        ssize_t send_to(const sockaddr_in & addr, const sihd::util::IArray & arr)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in), arr); }
        bool send_all_to(const sockaddr_in & addr, const sihd::util::IArray & arr)
            { return this->send_all_to((sockaddr *)&addr, sizeof(sockaddr_in), arr); }

        
        // IP from std::string
        bool bind(const std::string & ip, int port);
        bool connect(const std::string & ip, int port);
        ssize_t send_to(const std::string & ip, int port, const void *data, size_t size);
        bool send_all_to(const std::string & ip, int port, const void *data, size_t size);
        // sihd::util::IArray
        ssize_t send_to(const std::string & ip, int port, const sihd::util::IArray & arr)
            { return this->send_to(ip, port, arr.cbuf(), arr.byte_size()); }
        ssize_t send_all_to(const std::string & ip, int port, const sihd::util::IArray & arr)
            { return this->send_all_to(ip, port, arr.cbuf(), arr.byte_size()); }

        /*
            sihd::net::IpAddr

            Calls corresponding methods trying to find ipv4 addr with matching Socket's socktype and protocol
                then first ipv4.
        */
        bool bind(const IpAddr & addr);
        bool connect(const IpAddr & addr);
        // calls send_to_ip  or first IPV4 ip
        ssize_t send_to(const IpAddr & addr, const void *data, size_t size);
        bool send_all_to(const IpAddr & addr, const void *data, size_t size);
        ssize_t receive_from(IpAddr & addr, void *data, size_t size);
        // sihd::util::IArray
        ssize_t send_to(const IpAddr & addr, const sihd::util::IArray & arr)
            { return this->send_to(addr, arr.cbuf(), arr.byte_size()); }
        bool send_all_to(const IpAddr & addr, const sihd::util::IArray & arr)
            { return this->send_to(addr, arr.cbuf(), arr.byte_size()); }
        ssize_t receive_from(IpAddr & addr, sihd::util::IArray & arr)
            { return Socket::_adapt_array_size(arr, this->receive_from(addr, arr.buf(), arr.byte_capacity())); }

        // Operations on unix sockets //

        static std::string get_unix_socket_peername(int socket);
        bool bind_unix(const std::string & path);
        bool connect_unix(const std::string & path);
        ssize_t send_to_unix(const std::string & path, const void *data, size_t size);
        bool send_all_to_unix(const std::string & path, const void *data, size_t size);
        ssize_t receive_from_unix(std::string & path, void *data, size_t size);
        // sihd::util::IArray
        ssize_t send_to_unix(const std::string & path, const sihd::util::IArray & arr)
            { return this->send_to_unix(path, arr.cbuf(), arr.byte_size()); }
        bool send_all_to_unix(const std::string & path, const sihd::util::IArray & arr)
            { return this->send_all_to_unix(path, arr.cbuf(), arr.byte_size()); }
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
        void _init();
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