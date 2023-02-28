#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/signal.hpp>

#include <fmt/format.h>

namespace sihd::util
{

SigWatcher::SigWatcher(const std::vector<int> watch_signals, const Callback & callback, Timestamp polling_frequency):
    _signals(watch_signals),
    _stop(false)
{
    for (const int & signal : watch_signals)
    {
        signal::handle(signal);
    }
    _thread = std::thread([this, polling_frequency, callback] {
        int sig = this->_watch_signals(polling_frequency);
        if (sig >= 0)
            callback(sig);
    });
}

SigWatcher::~SigWatcher()
{
    this->stop();
}

void SigWatcher::stop()
{
    if (_stop.exchange(true))
        return;

    for (const int & signal : _signals)
    {
        signal::unhandle(signal);
    }

    if (_thread.joinable())
    {
        _thread.join();
    }
}

int SigWatcher::_watch_signals(Timestamp polling_frequency) const
{
    const std::chrono::nanoseconds sleep_duration = polling_frequency;

    while (!_stop)
    {
        for (int signal : _signals)
        {
            auto status = signal::status(signal);
            if (status.has_value() && status->received > 0)
                return signal;
        }

        std::this_thread::sleep_for(sleep_duration);
    }
    return -1;
}

} // namespace sihd::util
