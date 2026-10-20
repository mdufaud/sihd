#ifndef __SIHD_SYS_LINUX_CGROUP_HPP__
#define __SIHD_SYS_LINUX_CGROUP_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::cgroup
{

constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

struct CpuLimit
{
        std::optional<uint64_t> quota_us = std::nullopt;
        uint64_t period_us = 0;
};

bool is_v2_available();
std::optional<std::string> current_group();
std::optional<std::string> current_group_path();
bool is_container();

sihd::util::Result<std::optional<uint64_t>> memory_max();
sihd::util::Result<uint64_t> memory_current();
sihd::util::Result<CpuLimit> cpu_max();

sihd::util::Result<void> write_memory_max(std::optional<uint64_t> bytes);
sihd::util::Result<void> write_cpu_max(std::optional<uint64_t> quota_us, uint64_t period_us);

} // namespace sihd::sys::linux_::cgroup

#endif
