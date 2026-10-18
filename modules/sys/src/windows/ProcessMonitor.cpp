#include <processthreadsapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>

#include <map>

#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "../internal/process_monitor.hpp"
#include "internal/proc_metrics.hpp"

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

std::map<DWORD, uint32_t> thread_counts()
{
    std::map<DWORD, uint32_t> counts;
    HANDLE thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (thread_snap == INVALID_HANDLE_VALUE)
        return counts;
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    BOOL has_entry = Thread32First(thread_snap, &te32);
    while (has_entry)
    {
        ++counts[te32.th32OwnerProcessID];
        has_entry = Thread32Next(thread_snap, &te32);
    }
    CloseHandle(thread_snap);
    return counts;
}

} // namespace

std::vector<ProcessSample> ProcessMonitor::Impl::collect(const std::string & name_filter)
{
    std::vector<ProcessSample> fresh;
    const std::map<DWORD, uint32_t> threads = thread_counts();
    HANDLE process_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_snap == INVALID_HANDLE_VALUE)
        return fresh;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    BOOL has_entry = Process32First(process_snap, &pe32);
    while (has_entry)
    {
        bool selected = name_filter.empty();
        if (selected == false)
        {
            const auto matched = str::regex_match(pe32.szExeFile, name_filter);
            selected = !SIHD_UNEXPECTED_LOG(matched) && *matched;
        }
        if (selected)
        {
            ProcessSample sample;
            sample.name = pe32.szExeFile;
            sample.metrics.pid = static_cast<int>(pe32.th32ProcessID);
            sample.metrics.parent_pid = static_cast<int>(pe32.th32ParentProcessID);
            auto thread_count = threads.find(pe32.th32ProcessID);
            if (thread_count != threads.end())
                sample.metrics.thread_count = thread_count->second;
            HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
            if (handle != nullptr)
            {
                internal::process_metrics_of(handle, sample.metrics);
                CloseHandle(handle);
            }
            fresh.push_back(std::move(sample));
        }
        has_entry = Process32Next(process_snap, &pe32);
    }
    CloseHandle(process_snap);
    return fresh;
}

} // namespace sihd::sys
