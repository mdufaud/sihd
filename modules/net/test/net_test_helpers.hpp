#ifndef __SIHD_NET_TEST_HELPERS_HPP__
#define __SIHD_NET_TEST_HELPERS_HPP__

#include <sihd/sys/platform.hpp>

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
