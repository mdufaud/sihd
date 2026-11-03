#ifndef __SIHD_SYS_SLEEPWATCHER_HPP__
#define __SIHD_SYS_SLEEPWATCHER_HPP__

#include <functional>
#include <memory>
#include <string>

#include <sihd/sys/desktop.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys
{

class SleepWatcher
{
    public:
        enum class Event
        {
            suspending,
            resumed
        };

        using Callback = std::function<void(Event)>;

        static constexpr bool supported = desktop::supported;

        SleepWatcher();
        ~SleepWatcher();

        SleepWatcher(const SleepWatcher &) = delete;
        SleepWatcher & operator=(const SleepWatcher &) = delete;

        void set_app_name(const std::string & name);

        // the callback runs on an internal thread and never calls stop(), the system sleeps once it returned
        sihd::util::Result<void> start(const Callback & callback);
        sihd::util::Result<void> stop();
        bool is_running() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
