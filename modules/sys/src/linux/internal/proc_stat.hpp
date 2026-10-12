#ifndef __SIHD_SYS_LINUX_PROC_STAT_HPP__
#define __SIHD_SYS_LINUX_PROC_STAT_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include <sihd/sys/ProcessInfo.hpp>

namespace sihd::sys::internal
{

struct ProcIo
{
        uint64_t read_bytes = 0;
        uint64_t write_bytes = 0;
        uint64_t read_ops = 0;
        uint64_t write_ops = 0;
};

ProcessState process_state_from_char(char state);

// full metrics read; nullopt when the process is gone or its stat is unreadable
// 'name' receives the process comm when given
std::optional<ProcessMetrics> read_proc_metrics(int pid, std::string *name = nullptr);

// nullopt when the io file is unreadable (permission denied or old kernel)
std::optional<ProcIo> read_proc_io(int pid);

} // namespace sihd::sys::internal

#endif
