#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <new>

#include <sihd/sys/CrashHandler.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>

#if defined(__GLIBC__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <execinfo.h>
#endif

namespace sihd::sys
{

struct CrashHandler::Impl
{
        stack_t previous_stack;
};

// signal dispositions are process state: like signal.cpp, values live at file scope, guarded by g_installed
namespace
{

constexpr size_t alt_stack_size = 64 * 1024;

constexpr int stderr_fd = 2;

std::atomic<bool> g_installed {false};
std::string g_dump_path;
std::atomic<bool> g_rethrow {true};
struct sigaction g_previous[signal::max_signal];
alignas(16) static char g_alt_stack[alt_stack_size];

void write_all(int fd, const void *data, size_t size)
{
    const char *bytes = (const char *)data;
    while (size > 0)
    {
        const ssize_t written = ::write(fd, bytes, size);
        if (written <= 0)
            return;
        bytes += written;
        size -= (size_t)written;
    }
}

void write_str(int fd, const char *str)
{
    write_all(fd, str, strlen(str));
}

void write_num(int fd, uint64_t value)
{
    char buf[20];
    size_t i = sizeof(buf);
    while (true)
    {
        buf[--i] = '0' + (char)(value % 10);
        value /= 10;
        if (value == 0)
            break;
    }
    write_all(fd, buf + i, sizeof(buf) - i);
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
    size_t start = 2;
    while (start < sizeof(buf) - 1 && buf[start] == '0')
        ++start;
    write_all(fd, buf + start, sizeof(buf) - start);
}

void write_registers(int fd, ucontext_t *uc)
{
    if (uc == nullptr)
        return;
    uintptr_t pc = 0;
    uintptr_t sp = 0;
#if defined(__x86_64__)
    pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
#elif defined(__i386__)
    pc = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
    sp = (uintptr_t)uc->uc_mcontext.gregs[REG_ESP];
#elif defined(__aarch64__)
    pc = (uintptr_t)uc->uc_mcontext.pc;
    sp = (uintptr_t)uc->uc_mcontext.sp;
#elif defined(__arm__)
    pc = (uintptr_t)uc->uc_mcontext.arm_pc;
    sp = (uintptr_t)uc->uc_mcontext.arm_sp;
#endif
    write_str(fd, " pc=0x");
    write_hex(fd, pc);
    write_str(fd, " sp=0x");
    write_hex(fd, sp);
}

void write_report(int fd, int sig, siginfo_t *info, void *ctx)
{
    write_str(fd, "fatal signal ");
    write_num(fd, (uint64_t)sig);
    // a positive si_code is a kernel-generated fault, where si_addr is valid
    if (info != nullptr && info->si_code > 0)
    {
        write_str(fd, " at 0x");
        write_hex(fd, (uintptr_t)info->si_addr);
    }
    write_registers(fd, (ucontext_t *)ctx);
    write_str(fd, "\n");
    os::backtrace(fd);
}

void crash_handler(int sig, siginfo_t *info, void *ctx)
{
    write_report(stderr_fd, sig, info, ctx);
    if (g_dump_path.empty() == false)
    {
        const int dump_fd = open(g_dump_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dump_fd >= 0)
        {
            write_report(dump_fd, sig, info, ctx);
            close(dump_fd);
        }
    }
    // SA_RESETHAND already restored the default disposition
    if (g_rethrow.load(std::memory_order_relaxed))
        raise(sig);
    else
        _exit(128 + sig);
}

bool setup_alt_stack(stack_t *previous)
{
    stack_t stack;
    stack.ss_sp = g_alt_stack;
    stack.ss_size = sizeof(g_alt_stack);
    stack.ss_flags = 0;
    return sigaltstack(&stack, previous) == 0;
}

void restore_alt_stack(const stack_t & previous)
{
    sigaltstack(&previous, nullptr);
}

void restore_previous(int max_sig)
{
    for (int sig = 1; sig <= max_sig; ++sig)
    {
        if (signal::is_category_dump(sig))
            sigaction(sig, &g_previous[sig], nullptr);
    }
}

} // namespace

bool CrashHandler::install(const Options & options)
{
    if (_installed || g_installed.exchange(true))
        return false;
#if defined(__GLIBC__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
    // Load the unwinder before a signal can interrupt the allocator.
    void *frame;
    ::backtrace(&frame, 1);
#endif
    _impl = new (std::nothrow) Impl;
    if (_impl == nullptr)
    {
        g_installed.store(false);
        return false;
    }
    if (setup_alt_stack(&_impl->previous_stack) == false)
    {
        delete _impl;
        _impl = nullptr;
        g_installed.store(false);
        return false;
    }

    g_dump_path = options.dump_path;
    g_rethrow.store(options.rethrow);
    bool ok = true;
    int last_installed = 0;

    for (int sig = 1; sig < signal::max_signal; ++sig)
    {
        if (signal::is_category_dump(sig) == false)
            continue;
        struct sigaction action;
        std::memset(&action, 0, sizeof(action));
        action.sa_sigaction = crash_handler;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
        sigemptyset(&action.sa_mask);
        if (sigaction(sig, &action, &g_previous[sig]) != 0)
        {
            ok = false;
            break;
        }
        last_installed = sig;
    }

    if (ok == false)
    {
        restore_previous(last_installed);
        restore_alt_stack(_impl->previous_stack);
        delete _impl;
        _impl = nullptr;
        g_installed.store(false);
        return false;
    }
    _options = options;
    _installed = true;
    return true;
}

void CrashHandler::uninstall()
{
    if (_installed == false)
        return;
    restore_previous(signal::max_signal - 1);
    restore_alt_stack(_impl->previous_stack);
    delete _impl;
    _impl = nullptr;
    _installed = false;
    g_installed.store(false);
}

} // namespace sihd::sys
