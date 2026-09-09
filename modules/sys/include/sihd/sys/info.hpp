#ifndef __SIHD_SYS_INFO_HPP__
#define __SIHD_SYS_INFO_HPP__

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/Duration.hpp>

namespace sihd::sys::info
{

// bytes
std::optional<uint64_t> total_ram();
std::optional<uint64_t> available_ram();
std::optional<uint64_t> total_swap();
std::optional<uint64_t> free_swap();

// logical processors
uint32_t cpu_count();
std::optional<uint32_t> physical_core_count();

// nanoseconds spent on the cpus since boot
struct CpuTimes
{
        uint64_t user = 0;
        uint64_t nice = 0;
        uint64_t system = 0;
        uint64_t idle = 0;
        uint64_t iowait = 0;
        uint64_t irq = 0;
        uint64_t softirq = 0;
        uint64_t steal = 0;

        uint64_t total() const;
        uint64_t busy() const;
};

std::optional<CpuTimes> cpu_times();
std::vector<CpuTimes> per_core_cpu_times();

class CpuUsage
{
    public:
        CpuUsage();

        // samples again; ratios are computed since the previous sample
        bool sample();
        // percent busy since the previous sample; -1 when unavailable
        double usage() const;
        std::vector<double> per_core_usage() const;

    private:
        CpuTimes _global;
        std::vector<CpuTimes> _per_core;
        std::vector<double> _per_core_usage;
        double _usage = 0.0;
        bool _valid = false;
        bool _has_delta = false;
};

// hertz
std::optional<uint64_t> cpu_frequency();
std::optional<uint64_t> cpu_max_frequency();

struct OsInfo
{
        std::string name;
        std::string version;
        std::string kernel;
        std::string arch;
};

OsInfo os_info();

std::string hostname();
std::optional<std::string> domain();
sihd::util::Duration uptime();
// 1, 5 and 15 minutes averages; zeros when unavailable
std::array<double, 3> load_average();

struct HardwareIdentity
{
        std::string vendor;
        std::string model;
        std::string serial;
};

HardwareIdentity hardware_identity();

} // namespace sihd::sys::info

#endif
