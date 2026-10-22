#ifndef __SIHD_SYS_WINDOWS_JOB_HPP__
#define __SIHD_SYS_WINDOWS_JOB_HPP__

#include <cstdint>
#include <memory>
#include <optional>

#include <sihd/sys/fwd.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_
{

class Job
{
    public:
        struct Limits
        {
                std::optional<uint64_t> process_memory_bytes;
                std::optional<uint64_t> job_memory_bytes;
                std::optional<sihd::util::Duration> per_process_cpu_time;
                bool kill_on_close = true;
        };

        struct Accounting
        {
                uint32_t active_processes = 0;
                uint32_t total_processes = 0;
                sihd::util::Duration user_time;
                sihd::util::Duration kernel_time;
                uint64_t read_bytes = 0;
                uint64_t write_bytes = 0;
        };

        struct Event
        {
                enum class Kind
                {
                    job_time_expired,
                    process_time_expired,
                    active_process_limit,
                    active_process_zero,
                    new_process,
                    exit_process,
                    abnormal_exit_process,
                    process_memory_limit,
                    job_memory_limit,
                };

                Kind kind = Kind::active_process_zero;
                uint32_t process_id = 0;
        };

        Job();
        Job(Job && other);
        Job & operator=(Job && other);
        ~Job();

        Job(const Job &) = delete;
        Job & operator=(const Job &) = delete;

        sihd::util::Result<void> create();
        void close();

        bool is_open() const;

        sihd::util::Result<void> set_limits(const Limits & limits);
        sihd::util::Result<void> assign(const Process & process);
        sihd::util::Result<void> terminate();

        sihd::util::Result<Accounting> accounting() const;
        // negative timeout waits forever
        sihd::util::Result<Event> wait_event(sihd::util::Duration timeout);

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys::windows_

#endif
