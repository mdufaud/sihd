#include <array>
#include <atomic>
#include <csignal>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/signal.hpp>

#define FIRST_SIG 1
#define LAST_SIG 64

namespace sihd::util::signal
{

SIHD_NEW_LOGGER("sihd::util::signal");

namespace
{

SystemClock clock;

SigExitConfig exit_config;

volatile std::sig_atomic_t last_signal_received = -1;

std::array<SigStatus, LAST_SIG> signals_status;

void _check_exit_config(int sig)
{
    bool do_exit = (exit_config.on_stop && signal::is_category_stop(sig))
                   || (exit_config.on_termination && signal::is_category_termination(sig))
                   || (exit_config.on_dump && signal::is_category_dump(sig));

    if (!do_exit)
        return;

    if (exit_config.log_signal)
        SIHD_LOG(warning, "Signal caught '{}' - exiting", signal::name(sig));

    if (exit_config.exit_with_sig_number)
        exit(sig);

    exit(0);
}

void _signal_callback(int sig)
{
    last_signal_received = sig;

    _check_exit_config(sig);

    sig = sig - 1;
    if (sig >= 0 && (size_t)sig < signals_status.size())
    {
        auto & status = signals_status.at(sig);

        status.received++;
        status.time_received = clock.now();
    }
}

} // namespace

void set_exit_config(SigExitConfig config)
{
    exit_config = config;
}

bool is_category_stop(int sig)
{
    switch (sig)
    {
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return true;
        default:
            return false;
    }
}

bool is_category_termination(int sig)
{
    switch (sig)
    {
        case SIGALRM:
        case SIGHUP:
        case SIGINT:
        case SIGKILL:
        case SIGPIPE:
        case SIGPOLL:
        case SIGPROF:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGVTALRM:
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
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGQUIT:
        case SIGSEGV:
        case SIGSYS:
        case SIGTRAP:
        case SIGXCPU:
        case SIGXFSZ:
            return true;
        default:
            return false;
    }
}

bool handle(int sig)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_RESTART; // Process resume what it was doing before the interruption
    sa.sa_handler = _signal_callback;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sig, &sa, nullptr) != 0)
    {
        SIHD_LOG(error, "Sigaction failed: {}", signal::name(sig));
        return false;
    }
    return true;
}

bool unhandle(int sig)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sig, &sa, nullptr) != 0)
    {
        SIHD_LOG(error, "Sigaction failed: {}", signal::name(sig));
        return false;
    }
    return true;
}

int last_received()
{
    return last_signal_received;
}

std::optional<SigStatus> status(int sig)
{
    if (sig < FIRST_SIG || sig >= LAST_SIG)
        return std::nullopt;
    return signals_status.at(sig - FIRST_SIG);
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
    auto status = signal::status(sig);
    if (status.has_value() == false)
        return;
    status->received = 0;
    status->time_received = -1;
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
        return signame;
#endif
    return std::to_string(sig);
}

SigStatus::operator bool() const
{
    return received;
}

} // namespace sihd::util::signal
