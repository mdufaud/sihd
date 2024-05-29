#ifndef __SIHD_UTIL_SIGWATCHER_HPP__
#define __SIHD_UTIL_SIGWATCHER_HPP__

#include <atomic>
#include <functional>
#include <thread>

#include <sihd/util/Named.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/ThreadedService.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class SigWatcher: public Named,
                  public ThreadedService,
                  public Configurable,
                  public Observable<SigWatcher>,
                  protected IRunnable
{
    public:
        using Callback = std::function<void(int)>;

        SigWatcher(const std::string & name, Node *parent = nullptr);
        virtual ~SigWatcher();

        bool set_polling_frequency(double hz);

        bool add_signal(int sig);
        bool rm_signal(int sig);

        bool add_signals(const std::vector<int> & signals);
        bool rm_signals(const std::vector<int> & signals);

        bool is_running() const override;

        bool call_previous_handler(int sig);

        // filled with catched signals - call in observable
        const std::vector<int> & catched_signals() const { return _signals_to_handle; };

    protected:
        virtual bool on_start() override;
        virtual bool on_stop() override;
        virtual bool run() override;

    private:
        struct SigControl
        {
                SigHandler sig_handler;
                size_t last_received;
        };

        StepWorker _step_worker;

        std::mutex _mutex;
        std::vector<SigControl> _sig_controllers;

        std::vector<int> _signals;
        std::vector<int> _signals_to_handle;
};

} // namespace sihd::util

#endif
