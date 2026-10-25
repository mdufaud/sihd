#ifndef __SIHD_NET_SOCKET_HPP__
#define __SIHD_NET_SOCKET_HPP__

#include <optional>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::net
{

class Socket
{
    public:
        static constexpr int blocking_timeout = -1;

        Socket();
        Socket(int domain, int socket_type, int protocol);
        Socket(std::string_view domain, std::string_view socket_type, sihd::util::ZStringView protocol);
        // from existing socket - get_infos perform up to 3 getsockopt
        Socket(int socket, bool get_infos = true);
        Socket(int socket, int domain, int socket_type, int protocol);
        Socket(int socket, std::string_view domain, std::string_view socket_type, sihd::util::ZStringView protocol);
        Socket(Socket && other);
        virtual ~Socket();

        Socket(const Socket & other) = delete;
        Socket & operator=(const Socket & other) = delete;

        Socket & operator=(Socket && other);

        operator int() const;
        operator bool() const;

        // returns true if getpeername worked and the provided addr_len is still the same
        static sihd::util::Result<void> get_socket_peername(int socket, sockaddr *addr, socklen_t *addr_len);

        // return an IpAdress from socket using get_socket_peername; if ipv6 is true, checks for an ipv6 addr
        // first
        static sihd::util::Result<IpAddr> socket_ip(int socket, bool ipv6 = false);
        static sihd::util::Result<void> get_socket_infos(int socket, int *domain, int *type, int *protocol);
        // the pending SO_ERROR (0 = none)
        static sihd::util::Result<int> get_socket_error(int socket);

        static sihd::util::Result<void> close_socket(int socket);
        static sihd::util::Result<void> set_socket_tcp_nodelay(int socket, bool active);
        static sihd::util::Result<void> set_socket_blocking(int socket, bool active);
        static sihd::util::Result<void> set_socket_recv_timeout(int socket, int milliseconds);
        static sihd::util::Result<void> set_socket_reuseaddr(int socket, bool active);
        static sihd::util::Result<void> set_socket_broadcast(int socket, bool active);
        static sihd::util::Result<void> bind_socket_to_device(int socket, std::string_view name);
        static sihd::util::Result<bool> is_socket_tcp_nodelay(int socket);
        static sihd::util::Result<bool> is_socket_blocking(int socket);
        static sihd::util::Result<bool> is_socket_broadcast(int socket);
        static sihd::util::Result<void> set_socket_ttl(int socket, int ttl, bool ipv6 = false);
        static sihd::util::Result<void> set_socket_keepalive(int socket, bool active);
        static sihd::util::Result<bool> is_socket_keepalive(int socket);
        static sihd::util::Result<void> set_socket_reuseport(int socket, bool active);
        static sihd::util::Result<bool> is_socket_reuseport(int socket);
        static sihd::util::Result<void> set_socket_exclusiveaddruse(int socket, bool active);
        static sihd::util::Result<void> set_socket_rcvbuf(int socket, int size);
        static sihd::util::Result<void> set_socket_sndbuf(int socket, int size);
        static sihd::util::Result<int> get_socket_rcvbuf(int socket);
        static sihd::util::Result<int> get_socket_sndbuf(int socket);

        // an Error from the platform last error code: the message carries the system description
        static sihd::util::Error make_error(std::string_view message);

        sihd::util::Result<void> get_infos();

        sihd::util::Result<void> set_tcp_nodelay(bool active) const;
        sihd::util::Result<void> set_blocking(bool active) const;
        sihd::util::Result<void> set_recv_timeout(int milliseconds) const;
        sihd::util::Result<void> set_reuseaddr(bool active) const;
        sihd::util::Result<void> set_broadcast(bool active) const;
        sihd::util::Result<void> bind_to_device(std::string_view name) const;
        sihd::util::Result<bool> is_tcp_nodelay() const;
        sihd::util::Result<bool> is_blocking() const;
        sihd::util::Result<bool> is_broadcast() const;
        sihd::util::Result<void> set_ttl(int ttl) const;
        sihd::util::Result<void> set_keepalive(bool active) const;
        sihd::util::Result<bool> is_keepalive() const;
        sihd::util::Result<void> set_reuseport(bool active) const;
        sihd::util::Result<bool> is_reuseport() const;
        sihd::util::Result<void> set_exclusiveaddruse(bool active) const;
        sihd::util::Result<void> set_rcvbuf(int size) const;
        sihd::util::Result<void> set_sndbuf(int size) const;
        sihd::util::Result<int> get_rcvbuf() const;
        sihd::util::Result<int> get_sndbuf() const;

        sihd::util::Result<void> join_multicast(const IpAddr & group, const std::string & iface = "");
        sihd::util::Result<void> leave_multicast(const IpAddr & group, const std::string & iface = "");
        sihd::util::Result<void> set_multicast_ttl(int ttl);
        sihd::util::Result<void> set_multicast_loop(bool active);

        sihd::util::Result<void> open(std::string_view domain, std::string_view type, sihd::util::ZStringView protocol);
        sihd::util::Result<void> open(int domain, int socket_type, int protocol);
        virtual sihd::util::Result<void> close();
        virtual sihd::util::Result<void> shutdown() const;
        bool is_open() const;

        // MSG_NOSIGNAL by default where it exists: a dead peer gives EPIPE, not SIGPIPE
        virtual sihd::util::Result<size_t> send(sihd::util::ArrCharView view);
        sihd::util::Result<void> send_all(sihd::util::ArrCharView view);

        // 0 = peer closed the connection
        virtual sihd::util::Result<size_t> receive(void *data, size_t size);
        sihd::util::Result<size_t> receive(sihd::util::IArray & arr);

        sihd::util::Result<void> listen(uint16_t queue_size);
        sihd::util::Result<int> accept(sockaddr *addr, socklen_t *addr_len, int timeout_ms = blocking_timeout);
        sihd::util::Result<int> accept(int timeout_ms = blocking_timeout);
        sihd::util::Result<int> accept(IpAddr & ipaddr, int timeout_ms = blocking_timeout);

        sihd::util::Result<IpAddr> peeraddr(bool ipv6 = false) const;
        sihd::util::Result<int> local_port() const;

        sihd::util::Result<void> bind(const sockaddr *addr, socklen_t addr_len);
        virtual sihd::util::Result<void>
            connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms = blocking_timeout);
        sihd::util::Result<size_t> send_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view);
        sihd::util::Result<void> send_all_to(const sockaddr *addr, socklen_t addr_len, sihd::util::ArrCharView view);
        sihd::util::Result<size_t> receive_from(sockaddr *addr, socklen_t *addr_len, void *data, size_t size);
        sihd::util::Result<size_t> receive_from(sockaddr *addr, socklen_t *addr_len, sihd::util::IArray & arr);

        /*
            sihd::net::IpAddr
        */
        sihd::util::Result<void> bind(const IpAddr & addr);
        sihd::util::Result<void> connect(const IpAddr & addr, int timeout_ms = blocking_timeout);
        // calls send_to_ip  or first IPV4 ip
        sihd::util::Result<size_t> send_to(const IpAddr & addr, sihd::util::ArrCharView view);
        sihd::util::Result<void> send_all_to(const IpAddr & addr, sihd::util::ArrCharView view);
        sihd::util::Result<size_t> receive_from(IpAddr & addr, void *data, size_t size);
        sihd::util::Result<size_t> receive_from(IpAddr & addr, sihd::util::IArray & arr);

        static sihd::util::Result<std::string> unix_socket_peername(int socket);
        sihd::util::Result<void> bind_unix(std::string_view path);
        sihd::util::Result<void> connect_unix(std::string_view path);
        sihd::util::Result<size_t> send_to_unix(std::string_view path, sihd::util::ArrCharView view);
        sihd::util::Result<void> send_all_to_unix(std::string_view path, sihd::util::ArrCharView view);
        sihd::util::Result<size_t> receive_from_unix(std::string & path, void *data, size_t size);
        sihd::util::Result<size_t> receive_from_unix(std::string & path, sihd::util::IArray & arr);

        void set_send_flags(int flags);
        void set_receive_flags(int flags);

        int domain() const;
        int type() const;
        int protocol() const;
        int socket() const;

        bool is_unix() const;
        bool is_ip() const;
        bool is_ipv4() const;
        bool is_ipv6() const;

        sihd::util::Result<int> get_error() const;

        sihd::util::Result<void> reconnect(int timeout_ms = -1);

        const IpAddr & connect_addr() const;
        std::string connect_unix_path() const;

    protected:
        void _clear_socket_info();
        sihd::util::Result<void>
            _multicast_membership(const IpAddr & group, const std::string & iface, int ipv4_opt, int ipv6_opt);

        int _domain = -1;
        int _type = -1;
        int _protocol = -1;
        int _socket = -1;

        std::string _unix_bind_path;
        IpAddr _connect_addr;
        std::string _connect_unix_path;

        int _send_flags;
        int _rcv_flags;
        // tracked instance state: windows cannot probe a socket's blocking mode
        mutable bool _blocking = true;
};

} // namespace sihd::net

#endif
