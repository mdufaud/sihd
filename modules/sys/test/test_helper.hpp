#ifndef __SIHD_SYS_TEST_HELPER_HPP__
#define __SIHD_SYS_TEST_HELPER_HPP__

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/program.hpp>

#if defined(__SIHD_WINDOWS__)
# include <winsock2.h>

# include <io.h>
# include <ws2tcpip.h>
#else
# include <unistd.h>
#endif

namespace test
{

inline std::string bin_path(std::string_view name)
{
    namespace fs = sihd::sys::fs;
    // program::path() -> <build>/test/bin/sihd_sys[.exe], helpers -> <build>/bin/<name>[.exe]
    const std::string build_dir = fs::parent(fs::parent(fs::parent(sihd::sys::program::path())));
#if defined(__SIHD_WINDOWS__)
    return fs::combine(build_dir, fmt::format("bin/{}.exe", name));
#else
    return fs::combine(build_dir, fmt::format("bin/{}", name));
#endif
}

inline std::string helper_path()
{
    return bin_path("sihd_sys_test_helper");
}

struct ScopedEnv
{
        ScopedEnv(const std::string & name, const std::optional<std::string> & value): _name(name)
        {
            _previous = sihd::sys::env::get(name);
            if (value.has_value())
                (void)sihd::sys::env::set(name, *value);
            else
                (void)sihd::sys::env::unset(name);
        }

        ~ScopedEnv()
        {
            if (_previous.has_value())
                (void)sihd::sys::env::set(_name, *_previous);
            else
                (void)sihd::sys::env::unset(_name);
        }

        ScopedEnv(const ScopedEnv &) = delete;
        ScopedEnv & operator=(const ScopedEnv &) = delete;

        std::string _name;
        std::optional<std::string> _previous;
};

#if defined(__SIHD_WINDOWS__)

inline bool ensure_wsa()
{
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return started;
}

// WSAPoll only polls sockets: connected loopback TCP pair, fds[0] = read (server), fds[1] = write (client)
inline bool make_fd_pair(std::array<int, 2> & fds)
{
    if (!ensure_wsa())
        return false;

    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        return false;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    int addrlen = sizeof(addr);
    if (::bind(listener, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR
        || ::getsockname(listener, (sockaddr *)&addr, &addrlen) == SOCKET_ERROR
        || ::listen(listener, 1) == SOCKET_ERROR)
    {
        ::closesocket(listener);
        return false;
    }

    SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET)
    {
        ::closesocket(listener);
        return false;
    }

    if (::connect(client, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        ::closesocket(listener);
        ::closesocket(client);
        return false;
    }

    SOCKET server = ::accept(listener, nullptr, nullptr);
    ::closesocket(listener);
    if (server == INVALID_SOCKET)
    {
        ::closesocket(client);
        return false;
    }

    // disable Nagle so small writes are readable within the poll timeout
    const int one = 1;
    ::setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    ::setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));

    fds[0] = (int)server;
    fds[1] = (int)client;
    return true;
}

inline ssize_t read_fd(int fd, void *buf, size_t count)
{
    return ::recv((SOCKET)fd, (char *)buf, (int)count, 0);
}

inline ssize_t write_fd(int fd, const void *buf, size_t count)
{
    return ::send((SOCKET)fd, (const char *)buf, (int)count, 0);
}

inline void close_fd(int fd)
{
    ::closesocket((SOCKET)fd);
}

// plain crt file-descriptor io - windows sockets cannot use it
inline ssize_t read_file_fd(int fd, void *buf, size_t count)
{
    return ::_read(fd, buf, (unsigned int)count);
}

inline ssize_t write_file_fd(int fd, const void *buf, size_t count)
{
    return ::_write(fd, buf, (unsigned int)count);
}

inline off_t seek_file_fd(int fd, off_t offset, int whence)
{
    return ::_lseek(fd, (long)offset, whence);
}

#else

inline bool make_fd_pair(std::array<int, 2> & fds)
{
    return ::pipe(fds.data()) == 0;
}

inline ssize_t read_fd(int fd, void *buf, size_t count)
{
    return ::read(fd, buf, count);
}

inline ssize_t write_fd(int fd, const void *buf, size_t count)
{
    return ::write(fd, buf, count);
}

inline void close_fd(int fd)
{
    ::close(fd);
}

inline ssize_t read_file_fd(int fd, void *buf, size_t count)
{
    return ::read(fd, buf, count);
}

inline ssize_t write_file_fd(int fd, const void *buf, size_t count)
{
    return ::write(fd, buf, count);
}

inline off_t seek_file_fd(int fd, off_t offset, int whence)
{
    return ::lseek(fd, offset, whence);
}

#endif

} // namespace test

#endif
