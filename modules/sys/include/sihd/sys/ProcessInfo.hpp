#ifndef __SIHD_SYS_PROCESSINFO_HPP__
#define __SIHD_SYS_PROCESSINFO_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

enum class ProcessState
{
    unknown,
    idle,
    running,
    sleeping,
    disk_sleep,
    zombie,
    stopped,
    tracing,
    dead,
};

struct ProcessMetrics
{
        int pid = 0;
        int parent_pid = -1;
        ProcessState state = ProcessState::unknown;
        sihd::util::Duration user_time;
        sihd::util::Duration system_time;
        uint64_t rss_bytes = 0;
        // linux: total virtual memory; windows: committed bytes (PagefileUsage)
        uint64_t virtual_bytes = 0;
        uint32_t thread_count = 0;
        // linux: open file descriptors; windows: handle count; -1 when unavailable
        int32_t fd_count = -1;
        uint64_t io_read_bytes = 0;
        uint64_t io_write_bytes = 0;
        uint64_t io_read_ops = 0;
        uint64_t io_write_ops = 0;
};

class ProcessInfo
{
    public:
        static constexpr bool supported = !sihd::util::build::is_emscripten;

        ProcessInfo(int pid);
        ProcessInfo(std::string_view name);
        ProcessInfo(const ProcessInfo & process_info);
        ~ProcessInfo();

        static std::vector<ProcessInfo> get_all_process_from_name(std::string_view regex);
        static std::vector<int> pids();

        bool is_alive() const;

        int pid() const;
        const std::string & name() const;
        const std::string & cwd() const;
        const std::string & exe_path() const;
        const std::vector<std::string> & cmd_line() const;
        const std::vector<std::string> & env() const;
        sihd::util::Timestamp creation_time() const;
        ProcessMetrics metrics() const;

        bool write_into_stdin(sihd::util::ArrCharView view) const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
