#ifndef __SIHD_SYS_OS_HPP__
#define __SIHD_SYS_OS_HPP__

#include <expected>
#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys::os
{

// syscalls linux - windows
std::expected<void, sihd::util::Error> ioctl(int fd, unsigned long request, void *arg_ptr = nullptr);

std::expected<void, sihd::util::Error>
    setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);
std::expected<void, sihd::util::Error> getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen);

sihd::util::Timestamp boot_time();

#if defined(__SIHD_WINDOWS__)
// 100ns ticks since 1601-01-01 (FILETIME / ULARGE_INTEGER::QuadPart) -> unix epoch timestamp
sihd::util::Timestamp filetime_to_timestamp(uint64_t filetime_ticks);
sihd::util::Duration filetime_to_duration(uint64_t filetime_ticks);
// current system time (GetSystemTimePreciseAsFileTime, 100ns resolution)
sihd::util::Timestamp filetime_now();
#endif

std::expected<rlim_t, sihd::util::Error> max_fds();
pid_t pid();

// real backtrace only on glibc (linux) and windows; no-op elsewhere (musl/android/emscripten/apple)
constexpr bool backtrace_supported = sihd::util::build::is_windows || sihd::util::build::is_gnu;
// prints formatted backtrace into file descriptor
ssize_t backtrace(int fd, size_t backtrace_size = -1);
// bytes
std::expected<ssize_t, sihd::util::Error> peak_rss();
// bytes
ssize_t current_rss();

constexpr bool mem_lock_supported = sihd::util::build::is_windows
                                    || (sihd::util::build::is_unix && !sihd::util::build::is_emscripten);
std::expected<void, sihd::util::Error> mem_lock(const void *addr, size_t len);
std::expected<void, sihd::util::Error> mem_unlock(const void *addr, size_t len);

bool exists_in_path(std::string_view binary_name);

std::string error_str(int error_code);

// the last general os error: GetLastError on windows, errno on posix.
// win32 apis (LoadLibrary, CreateFile...) set this
int last_error();
std::string last_error_str();

// the last socket error: WSAGetLastError on windows, errno on posix.
// winsock apis (send, WSAPoll...) set this
int last_socket_error();
std::string last_socket_error_str();

bool is_run_by_valgrind();
bool is_run_by_qemu();
bool is_run_by_debugger();
bool is_run_by_wine();

} // namespace sihd::sys::os

#endif
