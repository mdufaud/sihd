#ifndef __SIHD_UTIL_STEPWORKER_HPP__
#define __SIHD_UTIL_STEPWORKER_HPP__

#include <atomic>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/ISteppable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/forward.hpp>

namespace sihd::util
{

class StepWorker: public Worker,
                  protected ISteppable
{
    public:
        using Callback = std::function<void()>;

        StepWorker();
        StepWorker(IRunnable *runnable);
        StepWorker(std::function<bool()> method);
        virtual ~StepWorker();

        bool set_frequency(double freq);
        void set_callback_setup(Callback && fun);
        void set_callback_teardown(Callback && fun);

        void pause_worker();
        void resume_worker();

        time_t nano_sleep_time() const { return _sleep_time; }
        double frequency() const;

    protected:
        bool run() override;
        bool step() override;
        bool on_worker_start() override;
        bool on_worker_stop() override;

    private:
        std::atomic<bool> _pause;
        time_t _sleep_time;

        Callback _callback_setup;
        Callback _callback_teardown;

        Waitable _pause_waitable;
        Waitable _sleep_waitable;
};

} // namespace sihd::util

#endif