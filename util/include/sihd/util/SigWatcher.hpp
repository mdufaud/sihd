#ifndef __SIHD_UTIL_SIGWATCHER_HPP__
#define __SIHD_UTIL_SIGWATCHER_HPP__

#include <atomic>
#include <functional>
#include <thread>

#include <sihd/util/SigHandler.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class SigWatcher
{
    public:
        using Callback = std::function<void(int)>;

        SigWatcher(const std::vector<int> signals,
                   Callback && callback,
                   Timestamp polling_frequency = std::chrono::milliseconds(100));
        ~SigWatcher();

        void stop();

    protected:

    private:
        int _watch_signals(Timestamp polling_frequency);

        std::thread _thread;
        std::vector<int> _signals;
        std::atomic<bool> _stop;

        std::vector<SigHandler> _sighandlers;

        Waitable _waitable;
};

} // namespace sihd::util

#endif
