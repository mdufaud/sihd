// clang-format off
# include <windows.h>
# include <dbghelp.h>
// clang-format on

#include <io.h> // _write

#include <atomic>
#include <cstring> // strlen
#include <new>

#include <sihd/sys/CrashHandler.hpp>
#include <sihd/sys/os.hpp>

namespace sihd::sys
{

struct CrashHandler::Impl
{
        LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;
};

// the unhandled exception filter is process state: the handler's values live
// at file scope, guarded by g_installed
namespace
{

constexpr int stderr_fd = 2;

std::atomic<bool> g_installed {false};
std::string g_dump_path;
std::atomic<bool> g_rethrow {true};

void write_all(int fd, const void *data, unsigned int size)
{
    const char *bytes = (const char *)data;
    while (size > 0)
    {
        const int written = _write(fd, bytes, size);
        if (written <= 0)
            return;
        bytes += written;
        size -= (unsigned int)written;
    }
}

void write_str(int fd, const char *str)
{
    write_all(fd, str, (unsigned int)strlen(str));
}

void write_hex(int fd, uintptr_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    char buf[18];
    buf[0] = '0';
    buf[1] = 'x';
    size_t i = sizeof(buf);
    while (i > 2)
    {
        buf[--i] = digits[value & 0xf];
        value >>= 4;
    }
    // skip leading zeros, keep at least one digit
    size_t start = 2;
    while (start < sizeof(buf) - 1 && buf[start] == '0')
        ++start;
    write_all(fd, buf + start, (unsigned int)(sizeof(buf) - start));
}

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS *info)
{
    // dump first: the symbolizer/stderr writes below can crash the filter (nested exception), losing the dump
    if (g_dump_path.empty() == false)
    {
        HANDLE file = CreateFileA(g_dump_path.c_str(),
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
        if (file == INVALID_HANDLE_VALUE)
            write_str(stderr_fd, "could not create the dump file\n");
        else
        {
            MINIDUMP_EXCEPTION_INFORMATION dump_info;
            dump_info.ThreadId = GetCurrentThreadId();
            dump_info.ExceptionPointers = info;
            dump_info.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(),
                              GetCurrentProcessId(),
                              file,
                              (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithProcessThreadData),
                              &dump_info,
                              nullptr,
                              nullptr);
            CloseHandle(file);
        }
    }
    if (info != nullptr && info->ExceptionRecord != nullptr)
    {
        write_str(stderr_fd, "fatal exception 0x");
        write_hex(stderr_fd, (uintptr_t)info->ExceptionRecord->ExceptionCode);
        write_str(stderr_fd, " at 0x");
        write_hex(stderr_fd, (uintptr_t)info->ExceptionRecord->ExceptionAddress);
        write_str(stderr_fd, "\n");
    }
    os::backtrace(stderr_fd);
    // rethrow chains to WER/JIT; otherwise terminate with the exception code,
    // symmetric to the unix rethrow=false (_exit 128+sig)
    if (g_rethrow.load(std::memory_order_relaxed))
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = (DWORD)-1;
    if (info != nullptr && info->ExceptionRecord != nullptr)
        code = info->ExceptionRecord->ExceptionCode;
    ExitProcess(code);
}

} // namespace

bool CrashHandler::install(const Options & options)
{
    if (_installed || g_installed.exchange(true))
        return false;
    // a debugger owns the crashes: do not interfere
    if (IsDebuggerPresent())
    {
        g_installed.store(false);
        return false;
    }
    _impl = new (std::nothrow) Impl;
    if (_impl == nullptr)
    {
        g_installed.store(false);
        return false;
    }
    g_dump_path = options.dump_path;
    g_rethrow.store(options.rethrow);
    _impl->previous_filter = SetUnhandledExceptionFilter(unhandled_filter);
    _options = options;
    _installed = true;
    return true;
}

void CrashHandler::uninstall()
{
    if (_installed == false)
        return;
    SetUnhandledExceptionFilter(_impl->previous_filter);
    delete _impl;
    _impl = nullptr;
    _installed = false;
    g_installed.store(false);
}

} // namespace sihd::sys
