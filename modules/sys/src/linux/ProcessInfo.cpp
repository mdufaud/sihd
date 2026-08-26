#include <functional>

#include <fmt/core.h>

#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

# include <unistd.h>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

bool proc_exists(int pid)
{
    return fs::is_dir(fmt::format("/proc/{}", pid));
}

std::string get_process_name_from_pid(int pid)
{
    auto line_opt = fs::read_all(fmt::format("/proc/{}/stat", pid));
    if (line_opt.has_value())
    {
        // The process name is the second field in the stat file, enclosed in parentheses
        const size_t start = line_opt->find_first_of('(') + 1;
        const size_t end = line_opt->find_last_of(')');
        return line_opt->substr(start, end - start);
    }
    return "";
}

void list_processes(std::function<bool(std::string_view, int)> predicate)
{
    for (const std::string & child : fs::children("/proc"))
    {
        if (str::regex_match(child, "\\d+/"))
        {
            int pid = std::stoi(child);
            if (proc_exists(pid))
            {
                std::string found_process_name = get_process_name_from_pid(pid);
                if (predicate(found_process_name, pid))
                {
                    return;
                }
            }
        }
    }
}

} // namespace

struct ProcessInfo::Impl
{
        Impl(int pid) { this->load(pid); };
        Impl(std::string_view name) { this->load(this->find_from_pid(name)); };

        int find_from_pid(std::string_view name);
        bool load(int pid);

        bool still_exists();

        void load_process_name();
        void load_time();
        void load_exe();

        void load_specific_process_infos();

        bool write_into_stdin(ArrCharView view) const;

        int pid = -1;
        std::string process_name;
        std::string exe_path;
        std::string cwd;
        std::vector<std::string> env;
        std::vector<std::string> cmd_line;
        Timestamp creation_time;
};

bool ProcessInfo::Impl::still_exists()
{
    return proc_exists(this->pid);
}

int ProcessInfo::Impl::find_from_pid(std::string_view name)
{
    int found_pid = -1;
    list_processes([&name, &found_pid](std::string_view process_name, int pid) {
        if (process_name == name)
        {
            found_pid = pid;
            return true;
        }
        return false;
    });
    return found_pid;
}

void ProcessInfo::Impl::load_time()
{
    Timestamp boot_time = os::boot_time();
    auto line_opt = fs::read_all(fmt::format("/proc/{}/stat", this->pid));
    auto status = line_opt.has_value() ? str::split(*line_opt, ' ') : std::vector<std::string> {};
    /**
     * From man stat:
     *           (22) starttime  %llu
                    The time the process started after system boot.  In
                    kernels before Linux 2.6, this value was expressed
                    in jiffies.  Since Linux 2.6, the value is expressed
                    in clock ticks (divide by sysconf(_SC_CLK_TCK)).
     */
    if (status.size() > 21)
    {
        this->creation_time
            = Duration(std::chrono::seconds(std::stoll(status[21]) / sysconf(_SC_CLK_TCK))) + boot_time;
    }
}

void ProcessInfo::Impl::load_specific_process_infos()
{
    auto cwd_opt = fs::read_link(fmt::format("/proc/{}/cwd", this->pid));
    this->cwd = cwd_opt.has_value() ? cwd_opt.value() : "";

    auto cmdline_opt = fs::read_all(fmt::format("/proc/{}/cmdline", this->pid));
    this->cmd_line = cmdline_opt.has_value() ? str::split(*cmdline_opt, '\0') : std::vector<std::string> {};

    auto env_opt = fs::read_all(fmt::format("/proc/{}/environ", this->pid));
    this->env = env_opt.has_value() ? str::split(*env_opt, '\0') : std::vector<std::string> {};
}

void ProcessInfo::Impl::load_exe()
{
    auto path_opt = fs::read_link(fmt::format("/proc/{}/exe", this->pid));
    this->exe_path = path_opt.has_value() ? path_opt.value() : "";
}

void ProcessInfo::Impl::load_process_name()
{
    if (this->process_name.empty())
        this->process_name = get_process_name_from_pid(this->pid);
}

bool ProcessInfo::Impl::load(int pid)
{
    if (pid < 0)
        return false;
    if (!proc_exists(pid))
        return false;
    this->pid = pid;
    this->load_process_name();
    this->load_exe();
    this->load_time();
    this->load_specific_process_infos();
    return true;
}

bool ProcessInfo::Impl::write_into_stdin(ArrCharView view) const
{
    return fs::write(fmt::format("/proc/{}/fd/0", this->pid), view) == view.size();
}

std::vector<ProcessInfo> ProcessInfo::get_all_process_from_name(const std::string & regex)
{
    std::vector<ProcessInfo> processes;
    list_processes([&regex, &processes](std::string_view process_name, int pid) {
        if (str::regex_match(process_name, regex))
        {
            processes.emplace_back(pid);
        }
        return false;
    });
    return processes;
}

ProcessInfo::ProcessInfo(int pid)
{
    _impl = std::make_unique<Impl>(pid);
}

ProcessInfo::ProcessInfo(std::string_view name)
{
    _impl = std::make_unique<Impl>(name);
}
ProcessInfo::ProcessInfo(const ProcessInfo & process_info)
{
    _impl = std::make_unique<Impl>(*process_info._impl);
}

ProcessInfo::~ProcessInfo() = default;

bool ProcessInfo::is_alive() const
{
    return _impl->still_exists();
}

int ProcessInfo::pid() const
{
    return _impl->pid;
}

const std::string & ProcessInfo::name() const
{
    return _impl->process_name;
}

const std::string & ProcessInfo::cwd() const
{
    return _impl->cwd;
}

const std::string & ProcessInfo::exe_path() const
{
    return _impl->exe_path;
}

const std::vector<std::string> & ProcessInfo::cmd_line() const
{
    return _impl->cmd_line;
}

const std::vector<std::string> & ProcessInfo::env() const
{
    return _impl->env;
}
bool ProcessInfo::write_into_stdin(ArrCharView view) const
{
    return _impl->write_into_stdin(view);
}

Timestamp ProcessInfo::creation_time() const
{
    return _impl->creation_time;
}

} // namespace sihd::sys
