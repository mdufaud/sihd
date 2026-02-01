#include <csignal>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/signal.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SigWaiter::SigWaiter(): _received_signal(false) {}

SigWaiter::SigWaiter(const Conf & conf): SigWaiter()
{
    _do_wait(conf.signal.value_or(SIGINT), conf.timeout);
}

SigWaiter::~SigWaiter() = default;

bool SigWaiter::received_signal() const
{
    return _received_signal;
}

bool SigWaiter::_do_wait(int sig, Timestamp timeout)
{
    _received_signal = false;
    const bool has_timeout = timeout > 0;

#if defined(__SIHD_WINDOWS__) || defined(__SIHD_ANDROID__)
    // Polling-based implementation for Windows and Android
    SteadyClock clock;
    const time_t begin = has_timeout ? clock.now() : 0;

    SigHandler sighandler(sig);
    if (!sighandler.is_handling())
    {
        SIHD_LOG(error, "SigWaiter: could not handle signal {}", sig);
        return false;
    }

    // Get initial received count
    const auto initial_status = signal::status(sig);
    const size_t initial_received
        = initial_status.has_value() ? initial_status->received.load(std::memory_order_relaxed) : 0;

    while (true)
    {
        const auto status = signal::status(sig);
        const size_t current_received
            = status.has_value() ? status->received.load(std::memory_order_relaxed) : 0;

        if (current_received > initial_received)
        {
            _received_signal = true;
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (has_timeout && ((clock.now() - begin) > timeout))
            return false;
    }
#else
    sigset_t oldmask;
    sigemptyset(&oldmask);
    sigset_t newmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, sig);

    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)
    {
        SIHD_LOG(error, "SigWaiter: sigprocmask failed: {}", os::last_error_str());
        return false;
    }

    bool result = false;

    if (has_timeout)
    {
        Timestamp remaining = timeout;
        SteadyClock clock;
        const time_t begin = clock.now();

        while (true)
        {
            remaining = timeout - (clock.now() - begin);
            if (remaining <= 0)
                break;

            timespec ts = remaining.ts();
            siginfo_t info;

            const int ret = sigtimedwait(&newmask, &info, &ts);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue; // Interrupted, retry with remaining time
                // EAGAIN = timeout, EINVAL = invalid args
                break;
            }

            if (info.si_signo == sig)
            {
                SIHD_TRACE("SigWaiter: signal {} received", sig);
                _received_signal = true;
                result = true;
                break;
            }
        }
    }
    else
    {
        while (true)
        {
            siginfo_t info;
            if (sigwaitinfo(&newmask, &info) == -1)
            {
                if (errno == EINTR)
                    continue;
                SIHD_LOG(error, "SigWaiter: sigwaitinfo failed: {}", os::last_error_str());
                break;
            }

            if (info.si_signo == sig)
            {
                SIHD_TRACE("SigWaiter: signal {} received", sig);
                _received_signal = true;
                result = true;
                break;
            }
        }
    }

    if (sigprocmask(SIG_SETMASK, &oldmask, nullptr) == -1)
    {
        SIHD_LOG(error, "SigWaiter: sigprocmask restore failed: {}", os::last_error_str());
    }

    return result;
#endif
}

} // namespace sihd::util
