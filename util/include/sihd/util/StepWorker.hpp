#ifndef __SIHD_UTIL_STEPWORKER_HPP__
# define __SIHD_UTIL_STEPWORKER_HPP__

# include <sihd/util/Worker.hpp>
# include <sihd/util/ISteppable.hpp>

namespace sihd::util
{

class StepWorker: public Worker, public ISteppable
{
    public:
        StepWorker(IRunnable *runnable = nullptr);
        virtual ~StepWorker();

        bool set_frequency(double freq);

        virtual void pause_worker();
        virtual void resume_worker();

        time_t nano_sleep_time() const { return _sleep_time; }
        double frequency() const { return time::to_hz(_sleep_time); };

    protected:
        virtual bool run() override;
        virtual bool step() override;
        virtual bool on_worker_start() override;
        virtual bool on_worker_stop() override;

        bool _pause;
        std::time_t _sleep_time;
        SteadyClock _clock;
        Waitable _pause_waitable;
};

}

#endif