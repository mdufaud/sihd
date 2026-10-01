#include <span>

#if !defined(__SIHD_WINDOWS__)
# include <csignal>
#endif

#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::signal
{

SIHD_NEW_LOGGER("sihd::sys::signal");

std::expected<void, Error> block_thread(int sig)
{
    const int sigs[] = {sig};
    return block_thread(sigs);
}

std::expected<void, Error> block_thread(std::span<const int> sigs)
{
    sigset_t set;
    sigemptyset(&set);
    for (int sig : sigs)
        sigaddset(&set, sig);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0)
        return std::unexpected(Error::from_errno("could not block signals"));
    return {};
}

std::expected<void, Error> unblock_thread(int sig)
{
    const int sigs[] = {sig};
    return unblock_thread(sigs);
}

std::expected<void, Error> unblock_thread(std::span<const int> sigs)
{
    sigset_t set;
    sigemptyset(&set);
    for (int sig : sigs)
        sigaddset(&set, sig);

    // drain any pending instances so unblocking does not deliver them
    struct timespec ts = {0, 0};
    while (sigtimedwait(&set, nullptr, &ts) > 0)
        ;

    if (pthread_sigmask(SIG_UNBLOCK, &set, nullptr) != 0)
        return std::unexpected(Error::from_errno("could not unblock signals"));
    return {};
}

// utilities

std::expected<void, Error> kill(pid_t pid, int sig)
{
    if (::kill(pid, sig) != 0)
        return std::unexpected(Error::from_errno("could not kill {} with signal {}", pid, sig));
    return {};
}

std::string name(int sig)
{
    char *signame = strsignal(sig);
    if (signame != nullptr)
        return std::string(signame);
    return std::to_string(sig);
}

} // namespace sihd::sys::signal
