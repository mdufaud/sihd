// clang-format off
# include <debugapi.h>
# include <winternl.h>
# include <windows.h>
// clang-format on

#include "../internal/info.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

#include <fmt/format.h>

#include <sihd/sys/info.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "internal/nt_dll.hpp"

namespace sihd::sys::info
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::info");

namespace
{

// 100ns ticks -> nanoseconds
uint64_t ticks_to_ns(uint64_t ticks)
{
    return ticks * 100;
}

uint64_t filetime_span_ns(const FILETIME & ft)
{
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return ticks_to_ns(uli.QuadPart);
}

bool memory_status(MEMORYSTATUSEX & status)
{
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) != 0;
}

// same layout as RTL_OSVERSIONINFOW, redefined to not depend on winternl.h's
// (possibly absent) definition
struct NtOsVersionInfo
{
        ULONG size;
        ULONG major;
        ULONG minor;
        ULONG build;
        ULONG platform;
        WCHAR csd_version[128];
};

// same layout as SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
struct ProcessorPerformance
{
        LARGE_INTEGER idle;
        LARGE_INTEGER kernel;
        LARGE_INTEGER user;
        LARGE_INTEGER dpc;
        LARGE_INTEGER interrupt;
        ULONG interrupt_count;
};

// same layout as PROCESSOR_POWER_INFORMATION
struct ProcessorPower
{
        ULONG number;
        ULONG max_mhz;
        ULONG current_mhz;
        ULONG mhz_limit;
        ULONG max_idle_state;
        ULONG current_idle_state;
};

using NtQuerySystemInformation_t = NTSTATUS(WINAPI *)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
using CallNtPowerInformation_t = NTSTATUS(WINAPI *)(ULONG, LPCVOID, ULONG, PVOID, ULONG);
using RtlGetVersion_t = LONG(WINAPI *)(NtOsVersionInfo *);

std::string registry_string(const char *key_path, const char *value_name)
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return "";
    char buffer[256];
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    std::string ret;
    if (RegQueryValueExA(key, value_name, nullptr, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS && type == REG_SZ
        && size > 0)
        ret.assign(buffer, strnlen(buffer, size));
    RegCloseKey(key);
    return ret;
}

// max is false for the current frequency, true for the max one
std::optional<uint64_t> power_info_mhz(bool max)
{
    void *ptr = internal::nt_dll_symbol("powrprof", "CallNtPowerInformation");
    if (ptr == nullptr)
        return std::nullopt;
    const auto call = (CallNtPowerInformation_t)ptr;
    const size_t count = cpu_count();
    if (count == 0)
        return std::nullopt;
    // ProcessorInformation
    std::vector<ProcessorPower> infos(count);
    if (call(11, nullptr, 0, infos.data(), (ULONG)(infos.size() * sizeof(ProcessorPower))) != 0)
        return std::nullopt;
    std::optional<uint64_t> mhz;
    for (const auto & info : infos)
    {
        const ULONG value = max ? info.max_mhz : info.current_mhz;
        if (value > 0)
            mhz = std::max(mhz.value_or(0), (uint64_t)value);
    }
    if (!mhz)
        return std::nullopt;
    return *mhz * 1'000'000ull;
}

std::string smb_string(const char *strings, const char *end, uint8_t index)
{
    if (index == 0)
        return "";
    for (uint8_t i = 1; strings < end && *strings != '\0'; ++i)
    {
        if (i == index)
            return std::string(strings, strnlen(strings, (size_t)(end - strings)));
        strings += strlen(strings) + 1;
    }
    return "";
}

} // namespace

std::optional<uint64_t> total_ram()
{
    MEMORYSTATUSEX status;
    if (!memory_status(status))
        return std::nullopt;
    return status.ullTotalPhys;
}

std::optional<uint64_t> available_ram()
{
    MEMORYSTATUSEX status;
    if (!memory_status(status))
        return std::nullopt;
    return status.ullAvailPhys;
}

std::optional<uint64_t> total_swap()
{
    MEMORYSTATUSEX status;
    if (!memory_status(status))
        return std::nullopt;
    // page file numbers include the physical memory
    if (status.ullTotalPageFile <= status.ullTotalPhys)
        return 0;
    return status.ullTotalPageFile - status.ullTotalPhys;
}

std::optional<uint64_t> free_swap()
{
    MEMORYSTATUSEX status;
    if (!memory_status(status))
        return std::nullopt;
    if (status.ullAvailPageFile <= status.ullAvailPhys)
        return 0;
    return status.ullAvailPageFile - status.ullAvailPhys;
}

uint32_t cpu_count()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

std::optional<uint32_t> physical_core_count()
{
    DWORD size = 0;
    GetLogicalProcessorInformation(nullptr, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return std::nullopt;
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> infos(size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (!GetLogicalProcessorInformation(infos.data(), &size))
        return std::nullopt;
    uint32_t count = 0;
    for (const auto & info : infos)
    {
        if (info.Relationship == RelationProcessorCore)
            ++count;
    }
    return count;
}

std::optional<CpuTimes> cpu_times()
{
    FILETIME idle;
    FILETIME kernel;
    FILETIME user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return std::nullopt;
    const uint64_t idle_ns = filetime_span_ns(idle);
    const uint64_t kernel_ns = filetime_span_ns(kernel);
    CpuTimes times;
    times.user = filetime_span_ns(user);
    // kernel time includes idle time
    times.system = kernel_ns > idle_ns ? kernel_ns - idle_ns : 0;
    times.idle = idle_ns;
    return times;
}

std::vector<CpuTimes> per_core_cpu_times()
{
    std::vector<CpuTimes> ret;
    void *ptr = internal::nt_dll_symbol("ntdll", "NtQuerySystemInformation");
    if (ptr == nullptr)
        return ret;
    const auto query = (NtQuerySystemInformation_t)ptr;
    ULONG size = 0;
    query(SystemProcessorPerformanceInformation, nullptr, 0, &size);
    if (size < sizeof(ProcessorPerformance))
        return ret;
    std::vector<ProcessorPerformance> infos(size / sizeof(ProcessorPerformance));
    if (query(SystemProcessorPerformanceInformation,
              infos.data(),
              (ULONG)(infos.size() * sizeof(ProcessorPerformance)),
              &size)
        != 0)
        return ret;
    ret.reserve(infos.size());
    for (const auto & perf : infos)
    {
        const uint64_t idle_ns = ticks_to_ns((uint64_t)perf.idle.QuadPart);
        const uint64_t kernel_ns = ticks_to_ns((uint64_t)perf.kernel.QuadPart);
        CpuTimes times;
        times.user = ticks_to_ns((uint64_t)perf.user.QuadPart);
        // kernel time includes idle time
        times.system = kernel_ns > idle_ns ? kernel_ns - idle_ns : 0;
        times.idle = idle_ns;
        ret.push_back(times);
    }
    return ret;
}

std::optional<uint64_t> cpu_frequency()
{
    return power_info_mhz(false);
}

std::optional<uint64_t> cpu_max_frequency()
{
    return power_info_mhz(true);
}

OsInfo os_info()
{
    OsInfo info;
    ULONG build = 0;
    void *ptr = internal::nt_dll_symbol("ntdll", "RtlGetVersion");
    if (ptr != nullptr)
    {
        NtOsVersionInfo version {};
        version.size = sizeof(version);
        if (((RtlGetVersion_t)ptr)(&version) == 0)
        {
            build = version.build;
            info.version = fmt::format("{}.{}.{}", version.major, version.minor, version.build);
            info.kernel = info.version;
        }
    }
    info.name = registry_string("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
    // ProductName still reports "Windows 10" on windows 11
    if (build >= 22000 && info.name.starts_with("Windows 10"))
        info.name = "Windows 11" + info.name.substr(10);
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info.arch = "x86_64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info.arch = "aarch64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info.arch = "x86";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            info.arch = "arm";
            break;
        default:
            info.arch = "unknown";
            break;
    }
    return info;
}

std::string hostname()
{
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (!GetComputerNameExA(ComputerNameDnsHostname, buf, &size))
        return "";
    return std::string(buf, size);
}

std::optional<std::string> domain()
{
    char buf[256];
    DWORD size = sizeof(buf);
    if (!GetComputerNameExA(ComputerNameDnsDomain, buf, &size))
        return std::nullopt;
    if (size == 0)
        return std::nullopt;
    return std::string(buf, size);
}

Duration uptime()
{
    return Duration(std::chrono::milliseconds(GetTickCount64()));
}

std::array<double, 3> load_average()
{
    // no load average concept on windows
    return {0.0, 0.0, 0.0};
}

HardwareIdentity hardware_identity()
{
    HardwareIdentity id;
    // 'RSMB' firmware table: 8 bytes RawSmbiosData header (method, major, minor,
    // revision, length) then the SMBIOS structures
    const DWORD signature = 0x52534D42; // 'RSMB' as a multi-character constant trips -Wmultichar
    const DWORD size = GetSystemFirmwareTable(signature, 0, nullptr, 0);
    if (size < 8)
        return id;
    std::vector<char> buffer(size);
    if (GetSystemFirmwareTable(signature, 0, buffer.data(), size) < size)
        return id;
    uint32_t table_size = 0;
    std::memcpy(&table_size, buffer.data() + 4, 4);
    const uint8_t *data = (const uint8_t *)buffer.data() + 8;
    const uint8_t *end = data + std::min<uint32_t>(table_size, size - 8);
    std::string sys_vendor, sys_model, sys_serial;
    std::string board_vendor, board_model, board_serial;
    const uint8_t *p = data;
    while (p + 4 <= end)
    {
        const uint8_t type = p[0];
        const uint8_t formatted_len = p[1];
        if (formatted_len < 4 || p + formatted_len > end)
            break;
        // string indices 0x04 manufacturer, 0x05 product, 0x07 serial are
        // shared by System Information (1) and Base Board (2)
        if ((type == 1 || type == 2) && formatted_len >= 8)
        {
            const char *strings = (const char *)p + formatted_len;
            std::string & vendor = type == 1 ? sys_vendor : board_vendor;
            std::string & model = type == 1 ? sys_model : board_model;
            std::string & serial = type == 1 ? sys_serial : board_serial;
            vendor = smb_string(strings, (const char *)end, p[0x04]);
            model = smb_string(strings, (const char *)end, p[0x05]);
            serial = smb_string(strings, (const char *)end, p[0x07]);
        }
        // skip the double-NUL terminated string table
        const uint8_t *q = p + formatted_len;
        while (q + 1 < end && !(q[0] == 0 && q[1] == 0))
            ++q;
        p = q + 2;
    }
    id.vendor = internal::dmi_identity_field(sys_vendor, board_vendor);
    id.model = internal::dmi_identity_field(sys_model, board_model);
    id.serial = internal::dmi_identity_field(sys_serial, board_serial);
    return id;
}

} // namespace sihd::sys::info
