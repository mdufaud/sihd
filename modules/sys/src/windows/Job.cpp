// clang-format off
#include <winsock2.h>
#include <windows.h>
// clang-format on

#include <algorithm>
#include <bit>
#include <utility>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/windows/Job.hpp>
#include <sihd/util/Clocks.hpp>

#include "internal/wait.hpp"
#include "internal/win_error.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::deadline_ms;
using sihd::sys::internal::Win32Error;

namespace sihd::sys::windows_
{

namespace
{

constexpr int64_t nanoseconds_per_tick = 100;

Duration duration_from_ticks(LARGE_INTEGER ticks)
{
    return Duration(ticks.QuadPart * nanoseconds_per_tick);
}

std::optional<Job::Event::Kind> event_kind_from_native(DWORD message)
{
    switch (message)
    {
        case JOB_OBJECT_MSG_END_OF_JOB_TIME:
            return Job::Event::Kind::job_time_expired;
        case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:
            return Job::Event::Kind::process_time_expired;
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT:
            return Job::Event::Kind::active_process_limit;
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
            return Job::Event::Kind::active_process_zero;
        case JOB_OBJECT_MSG_NEW_PROCESS:
            return Job::Event::Kind::new_process;
        case JOB_OBJECT_MSG_EXIT_PROCESS:
            return Job::Event::Kind::exit_process;
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
            return Job::Event::Kind::abnormal_exit_process;
        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:
            return Job::Event::Kind::process_memory_limit;
        case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT:
            return Job::Event::Kind::job_memory_limit;
        default:
            return std::nullopt;
    }
}

} // namespace

struct Job::Impl
{
        ~Impl() { this->close(); }

        void close()
        {
            if (job != nullptr)
            {
                CloseHandle(job);
                job = nullptr;
            }
            if (port != nullptr)
            {
                CloseHandle(port);
                port = nullptr;
            }
        }

        HANDLE job = nullptr;
        HANDLE port = nullptr;
};

Job::Job(): _impl(std::make_unique<Impl>()) {}

Job::Job(Job && other) = default;

Job & Job::operator=(Job && other) = default;

Job::~Job() = default;

Result<void> Job::create()
{
    this->close();
    _impl->job = CreateJobObjectA(nullptr, nullptr);
    if (_impl->job == nullptr)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not create job: {}", error));
    }

    _impl->port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (_impl->port == nullptr)
    {
        const Win32Error error = Win32Error::last();
        this->close();
        return std::unexpected(Error(error, "could not create job completion port: {}", error));
    }

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT association {.CompletionKey = _impl->job, .CompletionPort = _impl->port};
    if (SetInformationJobObject(_impl->job,
                                JobObjectAssociateCompletionPortInformation,
                                &association,
                                sizeof(association))
        == 0)
    {
        const Win32Error error = Win32Error::last();
        this->close();
        return std::unexpected(Error(error, "could not associate job completion port: {}", error));
    }
    return {};
}

void Job::close()
{
    _impl->close();
}

bool Job::is_open() const
{
    return _impl != nullptr && _impl->job != nullptr;
}

Result<void> Job::set_limits(const Limits & limits)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
    DWORD & flags = info.BasicLimitInformation.LimitFlags;
    if (limits.process_memory_bytes)
    {
        flags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        info.ProcessMemoryLimit = *limits.process_memory_bytes;
    }
    if (limits.job_memory_bytes)
    {
        flags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
        info.JobMemoryLimit = *limits.job_memory_bytes;
    }
    if (limits.per_process_cpu_time)
    {
        flags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
        info.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = limits.per_process_cpu_time->nanoseconds()
                                                                      / nanoseconds_per_tick;
    }
    if (limits.kill_on_close)
        flags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (SetInformationJobObject(_impl->job, JobObjectExtendedLimitInformation, &info, sizeof(info)) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not set job limits: {}", error));
    }
    return {};
}

Result<void> Job::assign(const Process & process)
{
    if (AssignProcessToJobObject(_impl->job, process.process()) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not assign process {} to job: {}", process.pid(), error));
    }
    return {};
}

Result<void> Job::terminate()
{
    if (TerminateJobObject(_impl->job, 1) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not terminate job: {}", error));
    }
    return {};
}

Result<Job::Accounting> Job::accounting() const
{
    JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION info {};
    if (QueryInformationJobObject(_impl->job, JobObjectBasicAndIoAccountingInformation, &info, sizeof(info), nullptr)
        == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not query job accounting: {}", error));
    }

    return Accounting {
        .active_processes = info.BasicInfo.ActiveProcesses,
        .total_processes = info.BasicInfo.TotalProcesses,
        .user_time = duration_from_ticks(info.BasicInfo.TotalUserTime),
        .kernel_time = duration_from_ticks(info.BasicInfo.TotalKernelTime),
        .read_bytes = info.IoInfo.ReadTransferCount,
        .write_bytes = info.IoInfo.WriteTransferCount,
    };
}

Result<Job::Event> Job::wait_event(Duration timeout)
{
    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (true)
    {
        const Duration remaining = timeout.nanoseconds() < 0 ? timeout : std::max(Duration(0), deadline - clock.now());
        DWORD message = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;
        if (GetQueuedCompletionStatus(_impl->port, &message, &key, &overlapped, deadline_ms(remaining)) == 0)
        {
            const Win32Error error = Win32Error::last();
            return std::unexpected(Error(error, "could not wait for a job event: {}", error));
        }

        const auto kind = event_kind_from_native(message);
        if (!kind)
            continue;
        // process messages carry the process id in place of the overlapped pointer
        return Event {.kind = *kind, .process_id = static_cast<uint32_t>(std::bit_cast<uintptr_t>(overlapped))};
    }
}

} // namespace sihd::sys::windows_
