#ifndef __SIHD_UTIL_SIGWATCHER_HPP__
#define __SIHD_UTIL_SIGWATCHER_HPP__

#include <atomic>
#include <functional>
#include <thread>

#include <sihd/util/AStepWorkerService.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class SigWatcher: public Named,
                  public AStepWorkerService,
                  public Configurable,
                  public Observable<SigWatcher>
{
    public:
        using Callback = std::function<void(int)>;

        SigWatcher(const std::string & name, Node *parent = nullptr);
        ~SigWatcher();

        bool add_signal(int sig);
        bool rm_signal(int sig);

        bool add_signals(const std::vector<int> & signals);
        bool rm_signals(const std::vector<int> & signals);

        bool call_previous_handler(int sig);

        // filled with catched signals - call in observable
        const std::vector<int> & catched_signals() const { return _signals_to_handle; };

    protected:
        bool on_work_setup() override;
        bool on_work_start() override;
        bool on_work_stop() override;
        bool on_work_teardown() override;

    private:
        struct SigControl
        {
                SigHandler sig_handler;
                size_t last_received;
        };

        std::mutex _mutex;
        std::vector<SigControl> _sig_controllers;

        std::vector<int> _signals;
        std::vector<int> _signals_to_handle;
};

} // namespace sihd::util

#endif
