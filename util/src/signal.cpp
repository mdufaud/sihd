#include <array>
#include <atomic>
#include <csignal>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/signal.hpp>

#define FIRST_SIG 1
#define LAST_SIG 64

#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

namespace sihd::util::signal
{

SIHD_NEW_LOGGER("sihd::util::signal");

namespace
{

SystemClock g_clock;

SigExitConfig g_exit_config;

// important volatile tag: the processor should NOT use its cache here
volatile std::sig_atomic_t g_last_signal_received = -1;

std::array<SigStatus, LAST_SIG> g_signals_status;

void _check_exit_config(int sig)
{
    const bool do_exit = (g_exit_config.on_stop && signal::is_category_stop(sig))
                         || (g_exit_config.on_termination && signal::is_category_termination(sig))
                         || (g_exit_config.on_dump && signal::is_category_dump(sig));

    if (!do_exit)
        return;

    if (g_exit_config.log_signal)
        SIHD_LOG(warning, "Signal caught '{}' ({}) - exiting", signal::name(sig), sig);

    if (g_exit_config.exit_with_sig_number)
        exit(sig);

    exit(0);
}

void _set_signal_received(int sig)
{
    sig = sig - 1;
    if (sig >= 0 && (size_t)sig < g_signals_status.size())
    {
        auto & status = g_signals_status.at(sig);

        status.received++;
        status.time_received = g_clock.now();
    }
}

void _reset_signal(int sig)
{
    sig = sig - 1;
    if (sig >= 0 && (size_t)sig < g_signals_status.size())
    {
        auto & status = g_signals_status.at(sig);

        status.received = 0;
        status.time_received = -1;
    }
}

void _signal_callback(int sig)
{
    // errno can be changed while handling signal - maybe breaking a program using it
    const int tmp_errno = errno;

    // there must be no mutex lock or anything outside of: man signal-safety
    g_last_signal_received = sig;
    _check_exit_config(sig);
    _set_signal_received(sig);

    // put errno back for program usage
    errno = tmp_errno;
}

} // namespace

void set_exit_config(const SigExitConfig & config)
{
    g_exit_config = config;
}

bool is_category_stop(int sig)
{
    switch (sig)
    {
#if !defined(__SIHD_WINDOWS__)
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return true;
#endif
        default:
            return false;
    }
}

bool is_category_termination(int sig)
{
    switch (sig)
    {
        case SIGTERM:
        case SIGINT:
#if !defined(__SIHD_WINDOWS__)
        case SIGALRM:
        case SIGHUP:
        case SIGKILL:
        case SIGPIPE:
        case SIGPOLL:
        case SIGPROF:
        case SIGUSR1:
        case SIGUSR2:
        case SIGVTALRM:
#endif
            return true;
        default:
            return false;
    }
}

bool is_category_dump(int sig)
{
    switch (sig)
    {
        case SIGABRT:
        case SIGILL:
        case SIGSEGV:
#if !defined(__SIHD_WINDOWS__)
        case SIGBUS:
        case SIGFPE:
        case SIGQUIT:
        case SIGSYS:
        case SIGTRAP:
        case SIGXCPU:
        case SIGXFSZ:
#endif
            return true;
        default:
            return false;
    }
}

bool ignore(int sig)
{
#if defined(__SIHD_WINDOWS__)
    sighandler_t previous_handler = ::signal(sig, SIG_IGN);
    if (previous_handler == SIG_ERR)
    {
        SIHD_LOG(error, "Error ignoring signal '{}': {}", sig, strerror(errno));
    }
    return previous_handler != SIG_ERR;
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sig, &sa, nullptr) != 0)
    {
        SIHD_LOG(error, "Error ignoring signal '{}': {}", sig, strerror(errno));
        return false;
    }
#endif
    return true;
}

bool handle(int sig)
{
#if defined(__SIHD_WINDOWS__)
    sighandler_t previous_handler = ::signal(sig, _signal_callback);
    if (previous_handler == SIG_ERR)
    {
        SIHD_LOG(error, "Error handling signal '{}': {}", sig, strerror(errno));
        return false;
    }
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_RESTART; // Process resume what it was doing before the interruption
    sa.sa_handler = _signal_callback;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sig, &sa, nullptr) != 0)
    {
        SIHD_LOG(error, "Error handling signal '{}': {}", sig, strerror(errno));
        return false;
    }
#endif
    return true;
}

bool unhandle(int sig)
{
#if defined(__SIHD_WINDOWS__)
    sighandler_t previous_handler = ::signal(sig, SIG_DFL);
    if (previous_handler == SIG_ERR)
    {
        SIHD_LOG(error, "Error unhandling signal '{}': {}", sig, strerror(errno));
        return false;
    }
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sig, &sa, nullptr) != 0)
    {
        SIHD_LOG(error, "Error unhandling signal '{}': {}", sig, strerror(errno));
        return false;
    }
#endif
    return true;
}

int last_received()
{
    return g_last_signal_received;
}

std::optional<SigStatus> status(int sig)
{
    if (sig < FIRST_SIG || sig >= LAST_SIG)
        return std::nullopt;
    return g_signals_status.at(sig - FIRST_SIG);
}

bool stop_received()
{
    for (int sig = FIRST_SIG; sig <= LAST_SIG; ++sig)
    {
        if (signal::is_category_stop(sig))
        {
            const auto status = signal::status(sig);
            if (status.has_value() && status->received > 0)
                return true;
        }
    }
    return false;
}

bool termination_received()
{
    for (int sig = FIRST_SIG; sig <= LAST_SIG; ++sig)
    {
        if (signal::is_category_termination(sig))
        {
            const auto status = signal::status(sig);
            if (status.has_value() && status->received > 0)
                return true;
        }
    }
    return false;
}

bool dump_received()
{
    for (int sig = FIRST_SIG; sig <= LAST_SIG; ++sig)
    {
        if (signal::is_category_dump(sig))
        {
            const auto status = signal::status(sig);
            if (status.has_value() && status->received > 0)
                return true;
        }
    }
    return false;
}

bool should_stop()
{
    return termination_received() || dump_received();
}

void reset_received(int sig)
{
    _reset_signal(sig);
}

void reset_all_received()
{
    for (int sig = FIRST_SIG; sig <= LAST_SIG; ++sig)
    {
        reset_received(sig);
    }
}

// utilities

bool kill(pid_t pid, int sig)
{
#if !defined(__SIHD_WINDOWS__)
    return ::kill(pid, sig) == 0;
#else
# pragma message("TODO os::kill")
    (void)pid;
    (void)sig;
    return false;
#endif
}

std::string name(int sig)
{
#if !defined(__SIHD_WINDOWS__)
    char *signame = strsignal(sig);
    if (signame != nullptr)
        return std::string(signame);
#endif
    return std::to_string(sig);
}

std::string status_str()
{
    std::string ret;

    for (int sig = FIRST_SIG; sig <= LAST_SIG; ++sig)
    {
        const auto status = signal::status(sig);
        if (status.has_value())
        {
            if (status->received > 0)
            {
                ret += fmt::format("{} -> {} ({})\n",
                                   signal::name(sig),
                                   status->received,
                                   Timestamp(status->time_received).day_str());
            }
        }
        else
        {
            ret += fmt::format("{} -> unknown\n", signal::name(sig));
        }
    }

    return ret;
}

SigStatus::operator bool() const
{
    return received;
}

} // namespace sihd::util::signal
