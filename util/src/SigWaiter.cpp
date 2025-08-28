#include <csignal>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SigWaiter::SigWaiter(const Conf & conf): _received_signal(false)
{
    const int wanted_signal = conf.signal.value_or(SIGINT);
    const bool has_timeout = conf.timeout > 0;

#if defined(__SIHD_WINDOWS__)
    SteadyClock clock;
    const time_t begin = has_timeout ? clock.now() : 0;

    SigHandler sighandler(wanted_signal);
    if (sighandler.is_handling())
    {
        while (true)
        {
            const auto status = signal::status(wanted_signal);

            _received_signal = status.has_value() && status->received > 0;
            if (_received_signal)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (has_timeout && ((clock.now() - begin) > conf.timeout))
                break;
        }
    }
#else
    sigset_t oldmask;
    sigemptyset(&oldmask);
    sigset_t newmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, wanted_signal);
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1)
    {
        SIHD_LOG(error, "sigprocmask failed: {}", os::last_error_str());
        return;
    }

    if (has_timeout)
    {
        Timestamp timeout = conf.timeout;

        SteadyClock clock;
        const time_t begin = has_timeout ? clock.now() : 0;
        while (1)
        {
            timeout = timeout - (clock.now() - begin);
            timespec ts = timeout.ts();
            siginfo_t info;
            if (sigtimedwait(&newmask, &info, &ts) == -1)
            {
                // errno EINTR signal interrupted
                if (errno == EINTR)
                {
                    continue;
                }
                // errno EAGAIN timed out
                // errno EINVAL timeout invalid
                break;
            }
            if (info.si_signo == wanted_signal)
            {
                SIHD_TRACE("Signal {} received", wanted_signal);
                _received_signal = true;
                break;
            }
        }
    }
    else
    {
        while (1)
        {
            siginfo_t info;
            if (sigwaitinfo(&newmask, &info) == -1)
            {
                // errno EINTR signal interrupted
                if (errno == EINTR)
                {
                    continue;
                }
                break;
            }
            if (info.si_signo == wanted_signal)
            {
                SIHD_TRACE("Signal {} received", wanted_signal);
                _received_signal = true;
                break;
            }
        }
    }
    if (sigprocmask(SIG_SETMASK, &oldmask, nullptr) == -1)
    {
        SIHD_LOG(error, "sigprocmask restore failed: {}", os::last_error_str());
        return;
    }
#endif
}

SigWaiter::SigWaiter(): SigWaiter(SigWaiter::Conf()) {}

SigWaiter::~SigWaiter() = default;

bool SigWaiter::received_signal() const
{
    return _received_signal;
}

} // namespace sihd::util
