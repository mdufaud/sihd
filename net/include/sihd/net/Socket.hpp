#ifndef __SIHD_NET_SOCKET_HPP__
# define __SIHD_NET_SOCKET_HPP__

# include <sihd/net/Ip.hpp>
# include <sihd/net/IpAddr.hpp>
# include <sihd/net/ISender.hpp>
# include <sihd/net/IReceiver.hpp>
# include <sihd/util/Configurable.hpp>


# if !defined(__SIHD_WINDOWS__)
#  include <sys/un.h>
#  include <netinet/tcp.h>
# else
#  define SHUT_RD SD_RECEIVE
#  define SHUT_WR SD_SEND
#  define SHUT_RDWR SD_BOTH
# endif

namespace sihd::net
{

class Socket:   virtual public ISender,
                virtual public IReceiver,
                public sihd::util::Configurable
{
    public:
        Socket();
        virtual ~Socket();

        // Class utilities for socket manipulation //

        // returns true if getpeername worked and the provided addr_len is still the same
        static bool   get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len);

        // return an IpAdress from socket using get_socket_peername; if ipv6 is true, checks for an ipv6 addr first
        static std::optional<IpAddr>   get_socket_ip(int socket, bool ipv6 = false);

        static bool set_socket_opt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror = false);
        static bool get_socket_opt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);
        static bool set_tcp_nodelay(int socket, bool active);
        static std::optional<bool> is_tcp_nodelay(int socket);

        // Operations on internal socket //

        bool open(const std::string & domain, const std::string & type, const std::string & protocol);
        bool open(int domain, int socket_type, int protocol);
        bool close();
        bool shutdown();
        bool is_open() { return _socket >= 0; }
        ssize_t send(void *data, size_t size);
        ssize_t receive(void *data, size_t size);
        bool listen(uint16_t queue_size);

        // fill addr and addr_len based on incoming connexion
        int accept(sockaddr *addr, socklen_t *addr_len);

        struct AcceptInfo
        {
            int socket;
            int port;
            std::string addr;
        };
        // if accept failed struct's socket will be negative
        AcceptInfo accept_info(bool ipv6 = false);


        // Utilities for internal socket //

        std::optional<IpAddr> get_peername(bool ipv6 = false) { return Socket::get_socket_ip(_socket, ipv6); }
        int get_local_port()
        {
            auto opt_ip = this->get_peername(true);
            return opt_ip ? opt_ip.value().port() : -1;
        }
        bool set_tcp_nodelay(bool active) { return Socket::set_tcp_nodelay(_socket, active); }
        std::optional<bool> is_tcp_nodelay() { return Socket::is_tcp_nodelay(_socket); }
        /*
        bool is_blocking();
        bool set_blocking(bool active);
        */

        // Operations on IP adresses //

        bool bind(sockaddr *addr, socklen_t addr_len);
        bool connect(sockaddr *addr, socklen_t addr_len);
        ssize_t send_to(sockaddr *addr, socklen_t addr_len, void *data, size_t size);
        ssize_t receive_from(sockaddr *addr, socklen_t addr_len, void *data, size_t size);

        ssize_t send_to(const sockaddr_in & addr, void *data, size_t size)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in), data, size); }
        ssize_t send_to(const sockaddr_in6 & addr, void *data, size_t size)
            { return this->send_to((sockaddr *)&addr, sizeof(sockaddr_in6), data, size); }

        ssize_t receive_from(const sockaddr_in & addr, void *data, size_t size)
            { return this->receive_from((sockaddr *)&addr, sizeof(sockaddr_in), data, size); }
        ssize_t receive_from(const sockaddr_in6 & addr, void *data, size_t size)
            { return this->receive_from((sockaddr *)&addr, sizeof(sockaddr_in6), data, size); }
        
        bool bind(const std::string & ip, int port);
        bool connect(const std::string & ip, int port);
        ssize_t send_to(const std::string & ip, void *data, size_t size);
        ssize_t receive_from(const std::string & ip, void *data, size_t size);

        bool bind(const IpAddr & addr);
        bool connect(const IpAddr & addr);
        // calls send_to_ip with matching Socket's socktype and protocol or first IPV4 ip
        ssize_t send_to(const IpAddr & addr, void *data, size_t size);
        // calls receive_from_ip with matching Socket's socktype and protocol or first IPV4 ip
        ssize_t receive_from(const IpAddr & addr, void *data, size_t size);

# if !defined(__SIHD_WINDOWS__)
        // Operations on unix sockets //

        static std::string   get_unix_socket_peername(int socket);
        bool bind_unix(const std::string & path);
        bool connect_unix(const std::string & path);
        ssize_t send_to_unix(const std::string & path, void *data, size_t size);
        ssize_t receive_from_unix(const std::string & path, void *data, size_t size);
# endif

        // Configuration //

        void set_verbose(bool active) { _verbose = active; }
        void set_send_flags(int flags) { _send_flags = flags; }
        void set_receive_flags(int flags) { _rcv_flags = flags; }

        // Getters //

        int domain() const { return _domain; }
        int type() const { return _type; }
        int protocol() const { return _protocol; }
        int socket() const { return _socket; }

    protected:
    
    private:
        void _init();
        void _clear_socket_info();

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