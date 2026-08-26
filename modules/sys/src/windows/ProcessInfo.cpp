#include <functional>

#include <fmt/core.h>

#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

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

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

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

void list_processes(std::function<bool(std::string_view, int)> predicate)
{
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
}

} // namespace

struct ProcessInfo::Impl
{
        Impl(int pid) { this->load(pid); };
        Impl(std::string_view name) { this->load(this->find_from_pid(name)); };
        ~Impl()
        {
            if (this->process_handle != INVALID_HANDLE_VALUE)
                CloseHandle(this->process_handle);
        }

        int find_from_pid(std::string_view name);
        bool load(int pid);

        bool still_exists();

        void load_process_name();
        void load_time();
        void load_exe();

        void load_specific_process_infos();

        bool write_into_stdin(ArrCharView view) const;

        HANDLE process_handle = INVALID_HANDLE_VALUE;
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
    if (this->process_handle != INVALID_HANDLE_VALUE)
        CloseHandle(this->process_handle);
    this->process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->pid);
    return this->process_handle != INVALID_HANDLE_VALUE;
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
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(this->process_handle, &creation_time, &exit_time, &kernel_time, &user_time))
    {
        ULARGE_INTEGER li;
        li.LowPart = creation_time.dwLowDateTime;
        li.HighPart = creation_time.dwHighDateTime;
        this->creation_time = os::filetime_to_timestamp(li.QuadPart);
    }
}

void ProcessInfo::Impl::load_specific_process_infos()
{
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

        wstr = get_memory_info(this->process_handle,
                               procParams.CommandLine.Buffer,
                               procParams.CommandLine.Length);
        str = str::to_str(wstr);
        this->cmd_line = str::split(str, ' ');
    }
}

void ProcessInfo::Impl::load_exe()
{
    CHAR buffer[MAX_PATH];
    if (GetModuleFileNameExA(this->process_handle, NULL, buffer, MAX_PATH))
        this->exe_path = buffer;
}

void ProcessInfo::Impl::load_process_name()
{
    CHAR buffer[MAX_PATH];
    if (GetModuleBaseNameA(this->process_handle, NULL, buffer, MAX_PATH))
        this->process_name = buffer;
}

bool ProcessInfo::Impl::load(int pid)
{
    if (pid < 0)
        return false;
    this->process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (this->process_handle == INVALID_HANDLE_VALUE)
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
