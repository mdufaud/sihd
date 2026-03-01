#ifndef __SIHD_SYS_OS_HPP__
#define __SIHD_SYS_OS_HPP__

#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::sys::os
{

// syscalls linux - windows
bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);

bool setsockopt(int socket,
                int level,
                int optname,
                const void *optval,
                socklen_t optlen,
                bool logerror = false);
bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);

sihd::util::Timestamp boot_time();

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

std::string last_error_str();
bool is_run_by_valgrind();
bool is_run_by_debugger();

} // namespace sihd::sys::os

#endif
