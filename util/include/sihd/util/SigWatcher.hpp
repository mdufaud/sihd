#ifndef __SIHD_UTIL_SIGWATCHER_HPP__
#define __SIHD_UTIL_SIGWATCHER_HPP__

#include <functional>
#include <mutex>
#include <span>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/platform.hpp>

// signalfd is only available on Linux (not Android, not Emscripten)
#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_HAS_SIGNALFD 1
#endif

namespace sihd::util
{

class SigWatcher: public Named,
                  public Configurable,
                  public Observable<SigWatcher>
{
    public:
        using Callback = std::function<void(int)>;

        SigWatcher(const std::string & name, Node *parent = nullptr);
        ~SigWatcher();

        bool set_polling_frequency(double frequency);

        bool add_signal(int sig);
        bool rm_signal(int sig);

        bool add_signals(std::span<const int> signals);
        bool rm_signals(std::span<const int> signals);

        bool call_previous_handler(int sig);

        bool start();
        bool stop();
        bool is_running() const;

        // filled with catched signals - call in observable
        const std::vector<int> & catched_signals() const { return _signals_to_handle; };

    private:
        struct SigControl
        {
                SigHandler sig_handler;
                size_t last_received;
        };

        void _run_polling_loop();
        void _notify_signals();

#if defined(SIHD_HAS_SIGNALFD)
        void _run_signalfd_loop();
        int _signalfd;
        sigset_t _sigset;
        Poll _poll;
#endif

        std::mutex _mutex;
        std::vector<SigControl> _sig_controllers;

        std::vector<int> _signals;
        std::vector<int> _signals_to_handle;

        Worker _worker;
        std::atomic<bool> _running;
        Timestamp _polling_interval;
};

} // namespace sihd::util

#undef SIHD_HAS_SIGNALFD

#endif
