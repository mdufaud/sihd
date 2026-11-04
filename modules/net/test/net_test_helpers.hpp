#ifndef __SIHD_NET_TEST_HELPERS_HPP__
#define __SIHD_NET_TEST_HELPERS_HPP__

#include <optional>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/platform.hpp>

namespace test
{

inline sihd::net::IpAddr bound_address(const sihd::net::Socket & socket)
{
    return sihd::net::IpAddr::localhost(socket.local_port().value_or(0));
}

// a loopback port free for tcp and udp right now, for a device that cannot bind port 0: use it at once
inline int free_loopback_port()
{
    sihd::net::Socket tcp;
    sihd::net::Socket udp;
    if (!tcp.open(AF_INET, SOCK_STREAM, IPPROTO_TCP) || !tcp.bind(sihd::net::IpAddr::localhost(0)))
        return 0;
    const int port = tcp.local_port().value_or(0);
    if (!udp.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP) || !udp.bind(sihd::net::IpAddr::localhost(port)))
        return 0;
    return port;
}

inline std::optional<sihd::net::NetInterface> find_interface(sihd::net::NetInterface::Kind kind, bool running)
{
    auto interfaces = sihd::net::NetInterface::get_all_interfaces();
    if (!interfaces)
        return std::nullopt;
    for (const auto & [name, netif] : *interfaces)
    {
        if (netif.kind() == kind && (!running || netif.running()))
            return netif;
    }
    return std::nullopt;
}

} // namespace test

#if !defined(__SIHD_WINDOWS__)

# include <sys/socket.h>

# include <csignal>

# include <sihd/net/Socket.hpp>
# include <sihd/sys/os.hpp>

namespace test
{

class ScopedSignalAction
{
    public:
        ScopedSignalAction(int signo, void (*handler)(int)): _signo(signo)
        {
            struct sigaction action = {};
            action.sa_handler = handler;
            _installed = sigaction(signo, &action, &_previous) == 0;
        }

        ~ScopedSignalAction()
        {
            if (_installed)
                sigaction(_signo, &_previous, nullptr);
        }

        ScopedSignalAction(const ScopedSignalAction &) = delete;
        ScopedSignalAction & operator=(const ScopedSignalAction &) = delete;

        bool installed() const { return _installed; }

    private:
        int _signo;
        bool _installed = false;
        struct sigaction _previous = {};
};

// close() then sends an RST instead of a FIN
inline bool set_reset_on_close(const sihd::net::Socket & sock)
{
    const struct linger abortive = {.l_onoff = 1, .l_linger = 0};
    return sihd::sys::os::setsockopt(sock.socket(), SOL_SOCKET, SO_LINGER, &abortive, sizeof(abortive)).has_value();
}

} // namespace test

#endif

#endif
