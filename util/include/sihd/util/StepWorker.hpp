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
        StepWorker(IRunnable *runnable = nullptr);
        virtual ~StepWorker();

        bool set_frequency(double freq);

        virtual void pause_worker();
        virtual void resume_worker();

        time_t nano_sleep_time() const { return _sleep_time; }
        double frequency() const;

    protected:
        virtual bool run() override;
        virtual bool step() override;
        virtual bool on_worker_start() override;
        virtual bool on_worker_stop() override;

        std::atomic<bool> _pause;
        time_t _sleep_time;
        SteadyClock _clock;
        Waitable _pause_waitable;
};

} // namespace sihd::util

#endif