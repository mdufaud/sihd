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

using _NtQueryInformationProcess = NTSTATUS(WINAPI *)(HANDLE ProcessHandle,
                                                      PROCESSINFOCLASS SystemInformationClass,
                                                      PVOID SystemInformation,
                                                      ULONG SystemInformationLength,
                                                      PULONG ReturnLength);

struct WindowsDriveLetterCurDir
{
        WORD Flags;
        WORD Length;
        ULONG TimeStamp;
        STRING DosPath;
};

struct WindowsUserProcessInfos
{
        ULONG MaximumLength;
        ULONG Length;
        ULONG Flags;
        ULONG DebugFlags;
        PVOID ConsoleHandle;
        ULONG ConsoleFlags;
        PVOID StdInputHandle;
        PVOID StdOutputHandle;
        PVOID StdErrorHandle;
        UNICODE_STRING CurrentDirectoryPath;
        PVOID CurrentDirectoryHandle;
        UNICODE_STRING DllPath;
        UNICODE_STRING ImagePathName;
        UNICODE_STRING CommandLine;
        PVOID Environment;
        ULONG StartingPositionLeft;
        ULONG StartingPositionTop;
        ULONG Width;
        ULONG Height;
        ULONG CharWidth;
        ULONG CharHeight;
        ULONG ConsoleTextAttributes;
        ULONG WindowFlags;
        ULONG ShowWindowFlags;
        UNICODE_STRING WindowTitle;
        UNICODE_STRING DesktopName;
        UNICODE_STRING ShellInfo;
        UNICODE_STRING RuntimeData;
        WindowsDriveLetterCurDir DLCurrentDirectory[32];
        ULONG EnvironmentSize;
};

#else

# include <unistd.h>

#endif

namespace sihd::util
{
SIHD_LOGGER;

namespace
{

#if defined(__SIHD_WINDOWS__)

bool read_process_memory(HANDLE & handle, WindowsUserProcessInfos & procParams)
{
    // https://stackoverflow.com/questions/1202653/check-for-environment-variable-in-another-process
    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength;

    void *ptr = (void *)GetProcAddress(GetModuleHandle("ntdll"), "NtQueryInformationProcess");
    if (ptr == nullptr)
        return false;
    _NtQueryInformationProcess fct = reinterpret_cast<_NtQueryInformationProcess>(ptr);
    if (fct == nullptr)
        return false;
    NTSTATUS status = fct(handle, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
    if (!NT_SUCCESS(status))
        return false;

    PEB peb;
    SIZE_T bytesRead;
    if (!ReadProcessMemory(handle, pbi.PebBaseAddress, &peb, sizeof(peb), &bytesRead))
        return false;

    if (!ReadProcessMemory(handle, peb.ProcessParameters, &procParams, sizeof(procParams), &bytesRead))
        return false;

    return true;
}

std::wstring get_memory_info(HANDLE & handle, PVOID addr, ULONG length)
{
    std::wstring wstr;
    wstr.resize((length / sizeof(wchar_t)) + 1);

    SIZE_T bytesRead;
    if (ReadProcessMemory(handle, addr, wstr.data(), length, &bytesRead))
    {
        wstr[bytesRead / sizeof(wchar_t)] = 0;
        return wstr;
    }
    return std::wstring {};
}

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
    HANDLE process_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_snap == INVALID_HANDLE_VALUE)
    {
        return;
    }

    PROCESSENTRY32 pe32;
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

        bool still_exists();

        void load_process_name();
        void load_time();
        void load_exe();

        void load_specific_process_infos();

        // void load_cmdline();
        // void load_cwd();
        // void load_env();

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

bool ProcessInfo::Impl::still_exists()
{
#if defined(__SIHD_WINDOWS__)
    if (this->process_handle != INVALID_HANDLE_VALUE)
        CloseHandle(this->process_handle);
    this->process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->pid);
    return this->process_handle != INVALID_HANDLE_VALUE;
#else
    return proc_exists(this->pid);
#endif
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

void ProcessInfo::Impl::load_specific_process_infos()
{
#if defined(__SIHD_WINDOWS__)
    WindowsUserProcessInfos procParams;
    if (read_process_memory(this->process_handle, procParams))
    {
        std::string str;
        std::wstring wstr;

        wstr = get_memory_info(this->process_handle,
                               procParams.CurrentDirectoryPath.Buffer,
                               procParams.CurrentDirectoryPath.Length);
        this->cwd = str::to_str(wstr);

        wstr = get_memory_info(this->process_handle, procParams.Environment, procParams.EnvironmentSize);
        str = str::to_str(wstr);
        this->env = str::split(str, '\0');

        wstr = get_memory_info(this->process_handle, procParams.CommandLine.Buffer, procParams.CommandLine.Length);
        str = str::to_str(wstr);
        this->cmd_line = str::split(str, ' ');
    }

#else
    auto cwd_opt = fs::read_link(fmt::format("/proc/{}/cwd", this->pid));
    this->cwd = cwd_opt.has_value() ? cwd_opt.value() : "";

    auto cmdline_opt = fs::read_all(fmt::format("/proc/{}/cmdline", this->pid));
    this->cmd_line = cmdline_opt.has_value() ? str::split(*cmdline_opt, '\0') : std::vector<std::string> {};

    auto env_opt = fs::read_all(fmt::format("/proc/{}/environ", this->pid));
    this->env = env_opt.has_value() ? str::split(*env_opt, '\0') : std::vector<std::string> {};
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
    this->load_exe();
    this->load_time();
    this->load_specific_process_infos();
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

} // namespace sihd::util
