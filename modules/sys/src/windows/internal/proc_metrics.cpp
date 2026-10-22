#include "proc_metrics.hpp"

#include <psapi.h>
#include <tlhelp32.h>

#include <sihd/sys/os.hpp>

namespace sihd::sys::internal
{

uint64_t filetime_ticks(const FILETIME & time)
{
    ULARGE_INTEGER li;
    li.LowPart = time.dwLowDateTime;
    li.HighPart = time.dwHighDateTime;
    return li.QuadPart;
}

uint32_t thread_count_of(DWORD pid)
{
    uint32_t count = 0;
    HANDLE thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (thread_snap == INVALID_HANDLE_VALUE)
        return 0;
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    BOOL has_entry = Thread32First(thread_snap, &te32);
    while (has_entry)
    {
        if (te32.th32OwnerProcessID == pid)
            ++count;
        has_entry = Thread32Next(thread_snap, &te32);
    }
    CloseHandle(thread_snap);
    return count;
}

void process_metrics_of(HANDLE handle, ProcessMetrics & metrics)
{
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(handle, &creation_time, &exit_time, &kernel_time, &user_time))
    {
        metrics.user_time = os::filetime_to_duration(filetime_ticks(user_time));
        metrics.system_time = os::filetime_to_duration(filetime_ticks(kernel_time));
    }
    PROCESS_MEMORY_COUNTERS memory_counters = {};

    if (GetProcessMemoryInfo(handle, &memory_counters, sizeof(memory_counters)))
    {
        metrics.rss_bytes = memory_counters.WorkingSetSize;
        metrics.virtual_bytes = memory_counters.PagefileUsage;
    }
    DWORD handle_count = 0;
    if (GetProcessHandleCount(handle, &handle_count))
        metrics.fd_count = static_cast<int32_t>(handle_count);
    IO_COUNTERS io_counters = {};
    if (GetProcessIoCounters(handle, &io_counters))
    {
        metrics.io_read_bytes = io_counters.ReadTransferCount;
        metrics.io_write_bytes = io_counters.WriteTransferCount;
        metrics.io_read_ops = io_counters.ReadOperationCount;
        metrics.io_write_ops = io_counters.WriteOperationCount;
    }
    DWORD exit_code = 0;
    if (GetExitCodeProcess(handle, &exit_code))
        // STILL_ACTIVE is indistinguishable from a process that exited with code 259
        metrics.state = exit_code == STILL_ACTIVE ? ProcessState::running : ProcessState::dead;
}

} // namespace sihd::sys::internal
