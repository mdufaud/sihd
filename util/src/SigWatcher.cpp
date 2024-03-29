#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

SigWatcher::SigWatcher(const std::vector<int> signals, Callback && callback, Timestamp polling_frequency):
    _signals(signals),
    _stop(false)
{
    std::vector<int> signals_to_unwatch;

    _sighandlers.reserve(_signals.size());
    for (const int & signal : _signals)
    {
        auto & sighandler = _sighandlers.emplace_back();
        if (sighandler.handle(signal) == false)
        {
            signals_to_unwatch.emplace_back(signal);
        }
    }

    if (signals_to_unwatch.empty() == false)
    {
        for (const int & signal : signals_to_unwatch)
            container::erase(_signals, signal);

        container::erase_if(_sighandlers, [&signals_to_unwatch](const auto & sighandler) {
            return container::contains(signals_to_unwatch, sighandler.handling_signal());
        });
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
    {
        auto l = _waitable.guard();
        if (_stop.exchange(true))
            return;
        _waitable.notify();
    }

    // unhandle signals
    _sighandlers.clear();

    if (_thread.joinable())
        _thread.join();
}

int SigWatcher::_watch_signals(Timestamp polling_frequency)
{
    thread::set_name("sig_watcher");

    while (_stop == false)
    {
        for (int signal : _signals)
        {
            auto status = signal::status(signal);
            if (status.has_value() && status->received > 0)
                return signal;
        }

        _waitable.wait_for(polling_frequency, [this] { return _stop == true; });
    }
    return -1;
}

} // namespace sihd::util
