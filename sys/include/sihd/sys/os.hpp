#ifndef __SIHD_SYS_OS_HPP__
#define __SIHD_SYS_OS_HPP__

#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::os
{

// syscalls linux - windows
bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror = false);
bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);

sihd::util::Timestamp boot_time();

#if defined(__SIHD_WINDOWS__)
// 100ns ticks since 1601-01-01 (FILETIME / ULARGE_INTEGER::QuadPart) -> unix epoch timestamp
sihd::util::Timestamp filetime_to_timestamp(uint64_t filetime_ticks);
// current system time (GetSystemTimePreciseAsFileTime, 100ns resolution)
sihd::util::Timestamp filetime_now();
#endif

bool is_root();
rlim_t max_fds();
pid_t pid();

// prints formatted backtrace into file descriptor
ssize_t backtrace(int fd, size_t backtrace_size = -1);
// bytes
ssize_t peak_rss();
// bytes
ssize_t current_rss();

bool exists_in_path(std::string_view binary_name);

std::string error_str(int error_code);
std::string last_error_str();

bool is_run_by_valgrind();
bool is_run_by_qemu();
bool is_run_by_debugger();

} // namespace sihd::sys::os

#endif
