#include <winsock2.h>

#include <array>
#include <cstring>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/sys/EventPoll.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

Error socket_error(std::string_view what)
{
    return Error(error_errno(os::last_socket_error()), fmt::format("{}: {}", what, os::last_socket_error_str()));
}

} // namespace

// WSAPoll cannot be woken: a datagram socket connected to itself interrupts the wait
struct EventPoll::Impl
{
        ~Impl()
        {
            if (wake_socket != INVALID_SOCKET)
                ::closesocket(wake_socket);
        }

        Result<void> open()
        {
            wake_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (wake_socket == INVALID_SOCKET)
                return std::unexpected(socket_error("could not create the wake up socket"));

            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            int addr_len = sizeof(addr);
            u_long non_blocking = 1;
            if (::bind(wake_socket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR
                || ::getsockname(wake_socket, (sockaddr *)&addr, &addr_len) == SOCKET_ERROR
                || ::connect(wake_socket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR
                || ::ioctlsocket(wake_socket, FIONBIO, &non_blocking) == SOCKET_ERROR)
                return std::unexpected(socket_error("could not set up the wake up socket"));
            return {};
        }

        void drain_wake()
        {
            std::array<char, 64> buffer;
            while (::recv(wake_socket, buffer.data(), (int)buffer.size(), 0) > 0)
            {
            }
        }

        SOCKET wake_socket = INVALID_SOCKET;
        bool waiting = false;
        std::vector<struct pollfd> polled;
};

EventPoll::EventPoll(): _impl(std::make_unique<Impl>())
{
    auto opened = _impl->open();
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

EventPoll::~EventPoll()
{
    if (this->is_running())
    {
        this->stop();
        this->service_wait_stop();
    }
}

// the wait polls a copy of the table, a change made during it needs a new wait
bool EventPoll::_watch(int, Interest, bool)
{
    if (_impl->waiting)
        this->_wake();
    return true;
}

void EventPoll::_unwatch(int)
{
    if (_impl->waiting)
        this->_wake();
}

void EventPoll::_wake()
{
    const char byte = 1;
    (void)::send(_impl->wake_socket, &byte, 1, 0);
}

int EventPoll::_wait(int milliseconds_timeout, std::vector<PollEvent> & ready)
{
    std::vector<struct pollfd> & polled = _impl->polled;
    {
        std::lock_guard lock(_interests_mutex);
        _impl->waiting = true;
        polled.clear();
        for (const auto & [fd, interest] : _interests)
        {
            const short events = static_cast<short>((interest.read ? POLLIN : 0) | (interest.write ? POLLOUT : 0));
            polled.push_back({.fd = static_cast<SOCKET>(fd), .events = events, .revents = 0});
        }
        polled.push_back({.fd = _impl->wake_socket, .events = POLLIN, .revents = 0});
    }

    const int ret = ::WSAPoll(polled.data(), (ULONG)polled.size(), milliseconds_timeout);
    {
        std::lock_guard lock(_interests_mutex);
        _impl->waiting = false;
    }
    if (ret <= 0)
        return ret;
    if (polled.back().revents != 0)
        _impl->drain_wake();
    for (size_t i = 0; i + 1 < polled.size(); ++i)
    {
        const short flags = polled[i].revents;
        if (flags == 0)
            continue;
        ready.push_back({.fd = static_cast<int>(polled[i].fd),
                         .readable = (flags & (POLLIN | POLLPRI)) != 0,
                         .writable = (flags & POLLOUT) != 0,
                         .error = (flags & (POLLERR | POLLNVAL)) != 0,
                         .closed = (flags & POLLHUP) != 0});
    }
    return ret;
}

} // namespace sihd::sys
