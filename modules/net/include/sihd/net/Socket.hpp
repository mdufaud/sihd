#ifndef __SIHD_NET_SOCKET_HPP__
#define __SIHD_NET_SOCKET_HPP__

#include <expected>
#include <optional>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::net
{

class Socket
{
    public:
        static constexpr int blocking_timeout = -1;

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
        static std::expected<void, sihd::util::Error>
            get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len);

        // return an IpAdress from socket using get_socket_peername; if ipv6 is true, checks for an ipv6 addr
        // first
        static std::optional<IpAddr> socket_ip(int socket, bool ipv6 = false);
        static bool get_socket_infos(int socket, int *domain, int *type, int *protocol);
        // nullopt when the probe failed, the pending SO_ERROR otherwise (0 = none)
        static std::optional<int> get_socket_error(int socket);

        static std::expected<void, sihd::util::Error> close_socket(int socket);
        static std::expected<void, sihd::util::Error> set_socket_tcp_nodelay(int socket, bool active);
        static std::expected<void, sihd::util::Error> set_socket_blocking(int socket, bool active);
        static std::expected<void, sihd::util::Error> set_socket_recv_timeout(int socket, int milliseconds);
        static std::expected<void, sihd::util::Error> set_socket_reuseaddr(int socket, bool active);
        static std::expected<void, sihd::util::Error> set_socket_broadcast(int socket, bool active);
        static std::expected<void, sihd::util::Error> bind_socket_to_device(int socket, std::string_view name);
        static bool is_socket_tcp_nodelay(int socket);
        static bool is_socket_blocking(int socket);
        static bool is_socket_broadcast(int socket);
        static std::expected<void, sihd::util::Error> set_socket_ttl(int socket, int ttl, bool ipv6 = false);
        static std::expected<void, sihd::util::Error> set_socket_keepalive(int socket, bool active);
        static bool is_socket_keepalive(int socket);
#ifdef SO_REUSEPORT
        static std::expected<void, sihd::util::Error> set_socket_reuseport(int socket, bool active);
        static bool is_socket_reuseport(int socket);
#endif
        static std::expected<void, sihd::util::Error> set_socket_rcvbuf(int socket, int size);
        static std::expected<void, sihd::util::Error> set_socket_sndbuf(int socket, int size);
        static int get_socket_rcvbuf(int socket);
        static int get_socket_sndbuf(int socket);

        // an Error from the platform last error code: the message carries the system description
        static sihd::util::Error make_error(std::string_view message);

        // Operations on internal socket //

        bool get_infos();

        std::expected<void, sihd::util::Error> set_tcp_nodelay(bool active) const;
        std::expected<void, sihd::util::Error> set_blocking(bool active) const;
        std::expected<void, sihd::util::Error> set_recv_timeout(int milliseconds) const;
        std::expected<void, sihd::util::Error> set_reuseaddr(bool active) const;
        std::expected<void, sihd::util::Error> set_broadcast(bool active) const;
        std::expected<void, sihd::util::Error> bind_to_device(std::string_view name) const;
        bool is_tcp_nodelay() const;
        bool is_blocking() const;
        bool is_broadcast() const;
        std::expected<void, sihd::util::Error> set_ttl(int ttl) const;
        std::expected<void, sihd::util::Error> set_keepalive(bool active) const;
        bool is_keepalive() const;
#ifdef SO_REUSEPORT
        std::expected<void, sihd::util::Error> set_reuseport(bool active) const;
        bool is_reuseport() const;
#endif
        std::expected<void, sihd::util::Error> set_rcvbuf(int size) const;
        std::expected<void, sihd::util::Error> set_sndbuf(int size) const;
        int get_rcvbuf() const;
        int get_sndbuf() const;

        std::expected<void, sihd::util::Error> join_multicast(const IpAddr & group, std::string_view iface = "");
        std::expected<void, sihd::util::Error> leave_multicast(const IpAddr & group, std::string_view iface = "");
        std::expected<void, sihd::util::Error> set_multicast_ttl(int ttl);
        std::expected<void, sihd::util::Error> set_multicast_loop(bool active);

        std::expected<void, sihd::util::Error>
            open(std::string_view domain, std::string_view type, std::string_view protocol);
        std::expected<void, sihd::util::Error> open(int domain, int socket_type, int protocol);
        virtual std::expected<void, sihd::util::Error> close();
        virtual std::expected<void, sihd::util::Error> shutdown() const;
        bool is_open() const { return _socket >= 0; }

        // A dead peer raises SIGPIPE: the process must ignore or handle it,
        // or pass MSG_NOSIGNAL through set_send_flags - the library never
        // touches signal dispositions.
        virtual std::expected<size_t, sihd::util::Error> send(sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> send_all(sihd::util::ArrCharView view);

        // 0 = peer closed the connection
        virtual std::expected<size_t, sihd::util::Error> receive(void *data, size_t size);
        std::expected<size_t, sihd::util::Error> receive(sihd::util::IArray & arr);

        std::expected<void, sihd::util::Error> listen(uint16_t queue_size);
        std::expected<int, sihd::util::Error>
            accept(sockaddr *addr, socklen_t *addr_len, int timeout_ms = blocking_timeout);
        std::expected<int, sihd::util::Error> accept(int timeout_ms = blocking_timeout);
        std::expected<int, sihd::util::Error> accept(IpAddr & ipaddr, int timeout_ms = blocking_timeout);

        // Utilities for internal socket //

        std::optional<IpAddr> peeraddr(bool ipv6 = false) const;
        int local_port() const;

        // Operations on IP adresses //

        // sockaddr
        std::expected<void, sihd::util::Error> bind(const sockaddr *addr, socklen_t addr_len);
        virtual std::expected<void, sihd::util::Error>
            connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms = blocking_timeout);
        std::expected<size_t, sihd::util::Error>
            send_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error>
            send_all_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view);
        std::expected<size_t, sihd::util::Error>
            receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size);
        std::expected<size_t, sihd::util::Error>
            receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr);

        /*
            sihd::net::IpAddr
        */
        std::expected<void, sihd::util::Error> bind(const IpAddr & addr);
        std::expected<void, sihd::util::Error> connect(const IpAddr & addr, int timeout_ms = blocking_timeout);
        // calls send_to_ip  or first IPV4 ip
        std::expected<size_t, sihd::util::Error> send_to(const IpAddr & addr, sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> send_all_to(const IpAddr & addr, sihd::util::ArrCharView view);
        std::expected<size_t, sihd::util::Error> receive_from(IpAddr & addr, void *data, size_t size);
        std::expected<size_t, sihd::util::Error> receive_from(IpAddr & addr, sihd::util::IArray & arr);

        // Operations on unix sockets //

        static std::string unix_socket_peername(int socket);
        std::expected<void, sihd::util::Error> bind_unix(std::string_view path);
        std::expected<void, sihd::util::Error> connect_unix(std::string_view path);
        std::expected<size_t, sihd::util::Error> send_to_unix(std::string_view path, sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> send_all_to_unix(std::string_view path, sihd::util::ArrCharView view);
        std::expected<size_t, sihd::util::Error> receive_from_unix(std::string & path, void *data, size_t size);
        std::expected<size_t, sihd::util::Error> receive_from_unix(std::string & path, sihd::util::IArray & arr);

        // Configuration //

        void set_verbose(bool active) { _verbose = active; }
        void set_send_flags(int flags) { _send_flags = flags; }
        void set_receive_flags(int flags) { _rcv_flags = flags; }

        // Getters //

        int domain() const { return _domain; }
        int type() const { return _type; }
        int protocol() const { return _protocol; }
        int socket() const { return _socket; }

        bool is_unix() const;
        bool is_ip() const;
        bool is_ipv4() const;
        bool is_ipv6() const;

        std::optional<int> get_error() const;
        std::string get_error_str() const;

        std::expected<void, sihd::util::Error> reconnect(int timeout_ms = -1);

        const IpAddr & connect_addr() const { return _connect_addr; }
        std::string connect_unix_path() const { return _connect_unix_path; }

    protected:
        void _clear_socket_info();
        std::expected<void, sihd::util::Error>
            _multicast_membership(const IpAddr & group, std::string_view iface, int ipv4_opt, int ipv6_opt);

        int _domain = -1;
        int _type = -1;
        int _protocol = -1;
        int _socket = -1;

        std::string _unix_bind_path;
        IpAddr _connect_addr;
        std::string _connect_unix_path;

        bool _verbose;
        int _send_flags;
        int _rcv_flags;
        // tracked instance state: windows cannot probe a socket's blocking mode
        mutable bool _blocking = true;
};

} // namespace sihd::net

#endif
