#include <csignal>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/signal.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SigWaiter::SigWaiter(const Conf & conf): _received_signal(false)
{
    _sig = conf.sig >= 0 ? conf.sig : SIGINT;
    const bool has_timeout = conf.timeout > 0;
    const std::chrono::nanoseconds sleep_duration = conf.polling_frequency;

    SteadyClock clock;
    const time_t begin = has_timeout ? clock.now() : 0;

    SigHandler sighandler(_sig);
    if (sighandler.is_handling())
    {
        while (true)
        {
            const auto status = signal::status(_sig);

            _received_signal = status.has_value() && status->received > 0;
            if (_received_signal)
                break;

            std::this_thread::sleep_for(sleep_duration);

            if (has_timeout && ((clock.now() - begin) > conf.timeout))
                break;
        }
    }
}

SigWaiter::SigWaiter(): SigWaiter(SigWaiter::Conf()) {}

SigWaiter::~SigWaiter() {}

bool SigWaiter::received_signal() const
{
    return _received_signal;
}

} // namespace sihd::util
