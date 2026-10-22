#ifndef __SIHD_SYS_WINDOWS_PROC_METRICS_HPP__
#define __SIHD_SYS_WINDOWS_PROC_METRICS_HPP__

#include <windows.h>

#include <sihd/sys/ProcessInfo.hpp>

namespace sihd::sys::internal
{

uint64_t filetime_ticks(const FILETIME & time);
uint32_t thread_count_of(DWORD pid);
void process_metrics_of(HANDLE handle, ProcessMetrics & metrics);

} // namespace sihd::sys::internal

#endif
