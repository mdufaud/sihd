#include <span>

#if !defined(__SIHD_WINDOWS__)
# include <csignal>
#endif

#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys::signal
{

SIHD_NEW_LOGGER("sihd::sys::signal");

bool block_thread(int sig)
{
    const int sigs[] = {sig};
    return block_thread(sigs);
}

bool block_thread(std::span<const int> sigs)
{
    sigset_t set;
    sigemptyset(&set);
    for (int sig : sigs)
        sigaddset(&set, sig);
    return pthread_sigmask(SIG_BLOCK, &set, nullptr) == 0;
}

bool unblock_thread(int sig)
{
    const int sigs[] = {sig};
    return unblock_thread(sigs);
}

bool unblock_thread(std::span<const int> sigs)
{
    sigset_t set;
    sigemptyset(&set);
    for (int sig : sigs)
        sigaddset(&set, sig);

    // drain any pending instances so unblocking does not deliver them
    struct timespec ts = {0, 0};
    while (sigtimedwait(&set, nullptr, &ts) > 0)
        ;

    return pthread_sigmask(SIG_UNBLOCK, &set, nullptr) == 0;
}

// utilities

bool kill(pid_t pid, int sig)
{
    return ::kill(pid, sig) == 0;
}

std::string name(int sig)
{
    char *signame = strsignal(sig);
    if (signame != nullptr)
        return std::string(signame);
    return std::to_string(sig);
}

} // namespace sihd::sys::signal
