#include <functional>

#include <fmt/format.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/ProcessInfo.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>

#if defined(__SIHD_WINDOWS__)
# include <windows.h>

# include <psapi.h>

# include <processthreadsapi.h>
# include <tlhelp32.h>
# include <winternl.h>

# include <tchar.h>

#else
#endif

namespace sihd::util
{
SIHD_LOGGER;

namespace
{

#if defined(__SIHD_WINDOWS__)

#else

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

#endif

void list_processes(std::function<bool(std::string_view, int)> predicate)
{
#if defined(__SIHD_WINDOWS__)
    HANDLE process_snap;
    PROCESSENTRY32 pe32;
    process_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_snap == INVALID_HANDLE_VALUE)
    {
        return;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(process_snap, &pe32))
    {
        CloseHandle(process_snap);
        return;
    }

    do
    {
        if (predicate(pe32.szExeFile, pe32.th32ProcessID))
        {
            CloseHandle(process_snap);
            return;
        }
    }
    while (Process32Next(process_snap, &pe32));

    CloseHandle(process_snap);
#else
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
#endif
}

} // namespace

struct ProcessInfo::Impl
{
        Impl(int pid) { this->load(pid); };
        Impl(std::string_view name) { this->load(this->find_from_pid(name)); };
        ~Impl()
        {
#if defined(__SIHD_WINDOWS__)
            if (this->process_handle != INVALID_HANDLE_VALUE)
                CloseHandle(this->process_handle);
#endif
        }

        int find_from_pid(std::string_view name);
        bool load(int pid);

        void load_process_name();
        void load_cmdline();
        void load_env();
        void load_exe();
        void load_cwd();
        void load_time();

        bool write_into_stdin(ArrCharView view) const;

#if defined(__SIHD_WINDOWS__)
        HANDLE process_handle = INVALID_HANDLE_VALUE;
#endif
        int pid = -1;
        std::string process_name;
        std::string exe_path;
        std::string cwd;
        std::vector<std::string> env;
        std::vector<std::string> cmd_line;
        Timestamp creation_time;
};

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
#if defined(__SIHD_WINDOWS__)
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(this->process_handle, &creation_time, &exit_time, &kernel_time, &user_time))
    {
        ULARGE_INTEGER li;
        li.LowPart = creation_time.dwLowDateTime;
        li.HighPart = creation_time.dwHighDateTime;
        // Convert from 100-nanosecond intervals since January 1, 1601 (UTC) to nanoseconds epoch time
        ULONGLONG epoch_time = (li.QuadPart - 116444736000000000ULL) * 100;
        this->creation_time = Timestamp(epoch_time);
    }
#else
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
            = Timestamp(std::chrono::seconds(std::stoll(status[21]) / sysconf(_SC_CLK_TCK))) + boot_time;
    }
#endif
}

void ProcessInfo::Impl::load_cwd()
{
#if defined(__SIHD_WINDOWS__)
#else
    auto path_opt = fs::read_link(fmt::format("/proc/{}/cwd", this->pid));
    this->cwd = path_opt.has_value() ? path_opt.value() : "";
#endif
}

void ProcessInfo::Impl::load_cmdline()
{
#if defined(__SIHD_WINDOWS__)
    PROCESSINFOCLASS pinfo = ProcessBasicInformation;
    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength;
    LONG status = NtQueryInformationProcess(this->process_handle, pinfo, &pbi, sizeof(pbi), &returnLength);
    if (status != 0)
        return;
    PPEB ppeb = pbi.PebBaseAddress;
    PPEB ppebCopy = (PPEB)malloc(sizeof(PEB));
    BOOL result = ReadProcessMemory(this->process_handle, ppeb, ppebCopy, sizeof(PEB), NULL);
    if (!result)
        return;

    PRTL_USER_PROCESS_PARAMETERS pRtlProcParam = ppebCopy->ProcessParameters;
    PRTL_USER_PROCESS_PARAMETERS pRtlProcParamCopy
        = (PRTL_USER_PROCESS_PARAMETERS)malloc(sizeof(RTL_USER_PROCESS_PARAMETERS));
    result = ReadProcessMemory(this->process_handle,
                               pRtlProcParam,
                               pRtlProcParamCopy,
                               sizeof(RTL_USER_PROCESS_PARAMETERS),
                               NULL);
    if (!result)
        return;
    PWSTR wBuffer = pRtlProcParamCopy->CommandLine.Buffer;
    USHORT len = pRtlProcParamCopy->CommandLine.Length;
    PWSTR wBufferCopy = (PWSTR)malloc(len);
    result = ReadProcessMemory(this->process_handle,
                               wBuffer,
                               wBufferCopy, // command line goes here
                               len,
                               NULL);
    if (!result)
        return;
    std::wstring wstr(wBufferCopy, len / sizeof(WCHAR));
    free(wBufferCopy);
    free(pRtlProcParamCopy);
    free(ppebCopy);
    this->cmd_line = str::split(str::to_str(wstr), ' ');
#else
    auto line_opt = fs::read_all(fmt::format("/proc/{}/cmdline", this->pid));
    this->cmd_line = line_opt.has_value() ? str::split(*line_opt, '\0') : std::vector<std::string> {};
#endif
}

void ProcessInfo::Impl::load_env()
{
#if defined(__SIHD_WINDOWS__)
#else
    auto line_opt = fs::read_all(fmt::format("/proc/{}/environ", this->pid));
    this->env = line_opt.has_value() ? str::split(*line_opt, '\0') : std::vector<std::string> {};
#endif
}

void ProcessInfo::Impl::load_exe()
{
#if defined(__SIHD_WINDOWS__)
    CHAR buffer[MAX_PATH];
    if (GetModuleFileNameExA(this->process_handle, NULL, buffer, MAX_PATH))
        this->exe_path = buffer;
#else
    auto path_opt = fs::read_link(fmt::format("/proc/{}/exe", this->pid));
    this->exe_path = path_opt.has_value() ? path_opt.value() : "";
#endif
}

void ProcessInfo::Impl::load_process_name()
{
#if defined(__SIHD_WINDOWS__)
    CHAR buffer[MAX_PATH];
    if (GetModuleBaseNameA(this->process_handle, NULL, buffer, MAX_PATH))
        this->process_name = buffer;
#else
    if (this->process_name.empty())
        this->process_name = get_process_name_from_pid(this->pid);
#endif
}

bool ProcessInfo::Impl::load(int pid)
{
    if (pid < 0)
        return false;
#if defined(__SIHD_WINDOWS__)
    this->process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (this->process_handle == INVALID_HANDLE_VALUE)
        return false;
#else
    if (!proc_exists(pid))
        return false;
#endif
    this->pid = pid;
    this->load_process_name();
    this->load_cmdline();
    this->load_env();
    this->load_exe();
    this->load_cwd();
    this->load_time();
    return true;
}

bool ProcessInfo::Impl::write_into_stdin(ArrCharView view) const
{
#if defined(__SIHD_WINDOWS__)
    HANDLE handle_stdin;
    if (!DuplicateHandle(GetCurrentProcess(),
                         GetStdHandle(STD_INPUT_HANDLE),
                         this->process_handle,
                         &handle_stdin,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS))
    {
        return false;
    }

    DWORD written;
    auto success = WriteFile(handle_stdin, view.data(), view.size(), &written, NULL);
    CloseHandle(handle_stdin);
    return success;
#else
    return fs::write(fmt::format("/proc/{}/fd/0", this->pid), view) == view.size();
#endif
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
ProcessInfo::ProcessInfo(const sihd::util::ProcessInfo & process_info)
{
    _impl = std::make_unique<Impl>(*process_info._impl);
}

ProcessInfo::~ProcessInfo() {}

bool ProcessInfo::is_alive() const
{
#if defined(__SIHD_WINDOWS__)
    return _impl->process_handle != INVALID_HANDLE_VALUE;
#else
    return _impl->pid >= 0;
#endif
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

} // namespace sihd::util
