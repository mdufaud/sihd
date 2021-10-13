#ifndef __SIHD_UTIL_STEPWORKER_HPP__
# define __SIHD_UTIL_STEPWORKER_HPP__

# include <sihd/util/Worker.hpp>
# include <sihd/util/ISteppable.hpp>

namespace sihd::util
{

class StepWorker: public Worker, virtual public ISteppable
{
    public:
        StepWorker(IRunnable *runnable);
        virtual ~StepWorker();

        bool set_frequency(double freq);

        virtual bool run() override;
        virtual bool step() override;

    protected:
        virtual bool on_worker_start() override;
        virtual bool on_worker_stop() override;

        std::time_t _sleep_time;
        SteadyClock _clock;
        Waitable    _waitable;
};

}

#endif 