#include "proc_stat.hpp"

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

#include <fmt/core.h>

#include <sihd/sys/fs.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys::internal
{

using namespace sihd::util;

namespace
{

// /proc/<pid>/stat offsets once the parenthesized comm is stripped: state is field 0
constexpr size_t stat_ppid = 1;
constexpr size_t stat_utime = 11;
constexpr size_t stat_stime = 12;
constexpr size_t stat_threads = 17;
constexpr size_t stat_vsize = 20;
constexpr size_t stat_rss = 21;

uint64_t to_u64(std::string_view value)
{
    // io lines are "key: 123": the value carries its leading space
    return str::to_unsigned(str::trim(value), 10).value_or(0);
}

Duration ticks_to_duration(uint64_t ticks, uint64_t clock_ticks)
{
    return Duration(std::chrono::nanoseconds(static_cast<int64_t>(ticks * 1000000000ULL / clock_ticks)));
}

std::optional<std::vector<std::string>> read_stat_fields(int pid, std::string *name)
{
    auto content = fs::read_all(fmt::format("/proc/{}/stat", pid));
    if (content.has_value() == false)
        return std::nullopt;
    // comm may hold any byte including spaces and ')': fields are parsed past the last ')'
    const size_t start = content->find_first_of('(');
    const size_t end = content->find_last_of(')');
    if (start == std::string::npos || end == std::string::npos || end < start)
        return std::nullopt;
    if (name != nullptr)
        *name = content->substr(start + 1, end - start - 1);
    std::vector<std::string> fields = str::split(str::trim(content->substr(end + 1)), ' ');
    if (fields.size() <= stat_rss)
        return std::nullopt;
    return fields;
}

void apply_stat_fields(ProcessMetrics & metrics,
                       const std::vector<std::string> & fields,
                       uint64_t clock_ticks,
                       uint64_t page_size)
{
    metrics.parent_pid = static_cast<int>(to_u64(fields[stat_ppid]));
    metrics.state = process_state_from_char(fields.front().empty() ? '\0' : fields.front().front());
    metrics.user_time = ticks_to_duration(to_u64(fields[stat_utime]), clock_ticks);
    metrics.system_time = ticks_to_duration(to_u64(fields[stat_stime]), clock_ticks);
    metrics.rss_bytes = to_u64(fields[stat_rss]) * page_size;
    metrics.virtual_bytes = to_u64(fields[stat_vsize]);
    metrics.thread_count = static_cast<uint32_t>(to_u64(fields[stat_threads]));
}

void apply_io_counters(ProcessMetrics & metrics, int pid)
{
    auto io = read_proc_io(pid);
    if (io.has_value() == false)
        return;
    metrics.io_read_bytes = io->read_bytes;
    metrics.io_write_bytes = io->write_bytes;
    metrics.io_read_ops = io->read_ops;
    metrics.io_write_ops = io->write_ops;
}

} // namespace

ProcessState process_state_from_char(char state)
{
    switch (state)
    {
        case 'R':
            return ProcessState::running;
        case 'S':
            return ProcessState::sleeping;
        case 'D':
            return ProcessState::disk_sleep;
        case 'Z':
            return ProcessState::zombie;
        case 'T':
            return ProcessState::stopped;
        case 't':
            return ProcessState::tracing;
        case 'I':
            return ProcessState::idle;
        case 'X':
        case 'x':
            return ProcessState::dead;
        default:
            return ProcessState::unknown;
    }
}

std::optional<ProcessMetrics> read_proc_metrics(int pid, std::string *name)
{
    auto fields = read_stat_fields(pid, name);
    if (fields.has_value() == false)
        return std::nullopt;
    const uint64_t clock_ticks = static_cast<uint64_t>(sysconf(_SC_CLK_TCK));
    const uint64_t page_size = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    ProcessMetrics metrics;
    metrics.pid = pid;
    apply_stat_fields(metrics, *fields, clock_ticks, page_size);
    const std::vector<std::string> fds = fs::children(fmt::format("/proc/{}/fd", pid));
    // an unreadable fd directory yields an empty listing: reported as unavailable
    metrics.fd_count = fds.empty() ? -1 : static_cast<int32_t>(fds.size());
    apply_io_counters(metrics, pid);
    return metrics;
}

std::optional<ProcIo> read_proc_io(int pid)
{
    auto lines = fs::read_lines(fmt::format("/proc/{}/io", pid));
    if (lines.has_value() == false)
        return std::nullopt;
    ProcIo io;
    for (const std::string & line : *lines)
    {
        const size_t sep = line.find(':');
        if (sep == std::string::npos)
            continue;
        const std::string_view key(line.data(), sep);
        const std::string value = line.substr(sep + 1);
        if (key == "read_bytes")
            io.read_bytes = to_u64(value);
        else if (key == "write_bytes")
            io.write_bytes = to_u64(value);
        else if (key == "syscr")
            io.read_ops = to_u64(value);
        else if (key == "syscw")
            io.write_ops = to_u64(value);
    }
    return io;
}

} // namespace sihd::sys::internal
