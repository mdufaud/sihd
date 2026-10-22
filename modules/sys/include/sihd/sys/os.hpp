#ifndef __SIHD_SYS_OS_HPP__
#define __SIHD_SYS_OS_HPP__

#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys::os
{

sihd::util::Result<void> ioctl(int fd, unsigned long request, void *arg_ptr = nullptr);

sihd::util::Result<void> setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);
sihd::util::Result<void> getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen);

enum class FdType
{
    unknown,
    regular,
    tty,
    pipe,
    socket,
};

sihd::util::Result<FdType> fd_type(int fd);
bool is_regular(int fd);
bool is_tty(int fd);
bool is_pipe(int fd);
bool is_socket(int fd);

sihd::util::Result<void> set_nonblocking(int fd, bool enable = true);
sihd::util::Result<size_t> bytes_available(int fd);

sihd::util::Result<int> dup(int fd);
sihd::util::Result<int> dup2(int fd, int to);

sihd::util::Timestamp boot_time();

#if defined(__SIHD_WINDOWS__)
// 100ns ticks since 1601-01-01 (FILETIME / ULARGE_INTEGER::QuadPart) -> unix epoch timestamp
sihd::util::Timestamp filetime_to_timestamp(uint64_t filetime_ticks);
sihd::util::Duration filetime_to_duration(uint64_t filetime_ticks);
sihd::util::Timestamp filetime_now();
#endif

sihd::util::Result<rlim_t> max_fds();
pid_t pid();

// real backtrace only on glibc (linux) and windows; no-op elsewhere (musl/android/emscripten/apple)
constexpr bool backtrace_supported = sihd::util::build::is_windows || sihd::util::build::is_gnu;
ssize_t backtrace(int fd, size_t backtrace_size = -1);
sihd::util::Result<ssize_t> peak_rss();
ssize_t current_rss();

constexpr bool mem_lock_supported = sihd::util::build::is_windows
                                    || (sihd::util::build::is_unix && !sihd::util::build::is_emscripten);
sihd::util::Result<void> mem_lock(const void *addr, size_t len);
sihd::util::Result<void> mem_unlock(const void *addr, size_t len);

bool exists_in_path(std::string_view binary_name);

std::string error_str(int error_code);

// the last general os error: GetLastError on windows, errno on posix.
int last_error();
std::string last_error_str();

// the last socket error: WSAGetLastError on windows, errno on posix.
int last_socket_error();
std::string last_socket_error_str();

bool is_run_by_valgrind();
bool is_run_by_qemu();
bool is_run_by_debugger();
bool is_run_by_wine();

} // namespace sihd::sys::os

#endif
