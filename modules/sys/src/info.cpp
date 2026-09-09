#include "internal/info.hpp"

#include <sihd/sys/info.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys::internal
{

using namespace sihd::util;

bool is_dmi_placeholder(std::string_view value)
{
    static const char *values[] = {"to be filled by o.e.m.",
                                   "system product name",
                                   "system manufacturer",
                                   "system serial number",
                                   "default string",
                                   "not specified",
                                   "not available",
                                   "none",
                                   "n/a",
                                   "empty",
                                   "0123456789"};
    for (const char *candidate : values)
    {
        if (str::iequals(value, candidate))
            return true;
    }
    return false;
}

std::string dmi_identity_field(std::string_view system, std::string_view board)
{
    if (!system.empty() && !is_dmi_placeholder(system))
        return std::string(system);
    return std::string(board);
}

} // namespace sihd::sys::internal

namespace sihd::sys::info
{

uint64_t CpuTimes::total() const
{
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

uint64_t CpuTimes::busy() const
{
    return total() - idle - iowait;
}

namespace
{

double usage_percent(const CpuTimes & from, const CpuTimes & to)
{
    // counters regress when cpus go offline
    if (to.total() <= from.total() || to.busy() < from.busy())
        return 0.0;
    return 100.0 * (to.busy() - from.busy()) / (to.total() - from.total());
}

} // namespace

CpuUsage::CpuUsage()
{
    if (const auto times = cpu_times())
    {
        _global = *times;
        _valid = true;
    }
    _per_core = per_core_cpu_times();
}

bool CpuUsage::sample()
{
    const auto times = cpu_times();
    if (!times)
    {
        _valid = false;
        _has_delta = false;
        return false;
    }
    const auto per_core = per_core_cpu_times();
    if (_valid)
    {
        _usage = usage_percent(_global, *times);
        _per_core_usage.clear();
        if (per_core.size() == _per_core.size())
        {
            for (size_t i = 0; i < per_core.size(); ++i)
                _per_core_usage.push_back(usage_percent(_per_core[i], per_core[i]));
        }
        _has_delta = true;
    }
    _global = *times;
    _per_core = std::move(per_core);
    _valid = true;
    return true;
}

double CpuUsage::usage() const
{
    return _has_delta ? _usage : -1.0;
}

std::vector<double> CpuUsage::per_core_usage() const
{
    return _has_delta ? _per_core_usage : std::vector<double> {};
}

} // namespace sihd::sys::info
