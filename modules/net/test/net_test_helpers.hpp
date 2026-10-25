#ifndef __SIHD_NET_TEST_HELPERS_HPP__
#define __SIHD_NET_TEST_HELPERS_HPP__

#include <optional>

#include <sihd/net/NetInterface.hpp>
#include <sihd/sys/platform.hpp>

namespace test
{

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
