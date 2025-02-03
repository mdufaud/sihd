#include <fmt/format.h>

#include <sihd/util/ProcessInfo.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

namespace
{

/**

#include <windows.h>
#include <psapi.h>


void get_command_line(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        WCHAR cmdLine[MAX_PATH];
        if (GetProcessCommandLine(hProcess, cmdLine, MAX_PATH)) {
            wprintf(L"Command Line: %s\n", cmdLine);
        }
        CloseHandle(hProcess);
    } else {
        printf("Failed to open process\n");
    }
}

void get_cwd(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        WCHAR cwd[MAX_PATH];
        if (GetProcessCwd(hProcess, cwd, MAX_PATH)) {
            wprintf(L"Current Working Directory: %s\n", cwd);
        }
        CloseHandle(hProcess);
    } else {
        printf("Failed to open process\n");
    }
}


void get_executable_path(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        WCHAR exePath[MAX_PATH];
        if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
            wprintf(L"Executable Path: %s\n", exePath);
        }
        CloseHandle(hProcess);
    } else {
        printf("Failed to open process\n");
    }
}

void get_process_status(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            printf("Working Set Size: %zu bytes\n", pmc.WorkingSetSize);
        }
        CloseHandle(hProcess);
    } else {
        printf("Failed to open process\n");
    }
}





void write_to_process_stdin(DWORD pid, const char *input) {
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    if (hProcess == NULL) {
        printf("Failed to open process\n");
        return;
    }

    HANDLE hStdIn;
    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE),
                         hProcess, &hStdIn, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        printf("Failed to duplicate handle\n");
        CloseHandle(hProcess);
        return;
    }

    DWORD written;
    if (!WriteFile(hStdIn, input, strlen(input), &written, NULL)) {
        printf("Failed to write to STDIN\n");
    } else {
        printf("Successfully wrote to STDIN\n");
    }

    CloseHandle(hStdIn);
    CloseHandle(hProcess);
}








 */

bool proc_exists(int pid)
{
    return fs::is_dir(fmt::format("/proc/{}", pid));
}

std::string get_process_name_from_pid(int pid)
{
    auto line_opt = fs::read_line(fmt::format("/proc/{}/stat", pid), 0);
    if (line_opt.has_value())
    {
        // The process name is the second field in the stat file, enclosed in parentheses
        const size_t start = line_opt->find('(') + 1;
        const size_t end = line_opt->find(')');
        return line_opt->substr(start, end - start);
    }
    return "";
}

} // namespace

struct ProcessInfo::Impl
{
        Impl(int pid) { this->load(pid); };
        Impl(std::string_view name) { this->load(this->find_pid(name)); };
        ~Impl() {}

        int find_pid(std::string_view name);
        bool load(int pid);

        void load_cmdline();
        void load_env();
        void load_exe();

        int pid;
        std::string process_name;
        std::string exe_path;
        std::vector<std::string> env;
        std::vector<std::string> cmd_line;
};

int ProcessInfo::Impl::find_pid(std::string_view name)
{
    for (const std::string & child : fs::children("/proc"))
    {
        if (str::regex_match(child, "\\d+"))
        {
            int pid = std::stoi(child);
            if (proc_exists(pid))
            {
                std::string found_process_name = get_process_name_from_pid(pid);
                if (found_process_name == name)
                {
                    this->process_name = std::move(found_process_name);
                    return pid;
                }
            }
        }
    }
    return -1;
}

bool ProcessInfo::Impl::load(int pid)
{
    if (pid < 0)
        return false;
    if (!proc_exists(pid))
        return false;
    this->pid = pid;
    if (this->process_name.empty())
        this->process_name = get_process_name_from_pid(pid);
    return true;
}

ProcessInfo::ProcessInfo(int pid)
{
    _impl = std::make_unique<Impl>(pid);
}

ProcessInfo::ProcessInfo(std::string_view name)
{
    _impl = std::make_unique<Impl>(name);
}

ProcessInfo::~ProcessInfo() {}

int ProcessInfo::pid() const
{
    return _impl->pid;
}

const std::string & ProcessInfo::name() const
{
    return _impl->process_name;
}

} // namespace sihd::util
