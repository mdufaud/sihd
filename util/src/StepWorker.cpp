#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Time.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/ScopedModifier.hpp>
#include <sihd/util/Clocks.hpp>

namespace sihd::util
{

SIHD_LOGGER;

StepWorker::StepWorker(IRunnable *runnable): Worker(runnable), _pause(false), _pausing(false), _sleep_time(0)
{
    this->add_conf("frequency", &StepWorker::set_frequency);
}

StepWorker::~StepWorker()
{
    this->stop_worker();
}

bool    StepWorker::set_frequency(double frequency)
{
    if (frequency <= 0.0)
    {
        SIHD_LOG(error, "StepWorker: frequency " << frequency << " cannot be negative");
        return false;
    }
    _sleep_time = Time::freq(frequency);
    return true;
}

double  StepWorker::frequency() const
{
    return Time::to_hz(_sleep_time);
}

bool    StepWorker::run()
{
    bool ret = true;
    time_t now = 0;
    time_t after = 0;
    if (_pause)
    {
        ScopedModifier m(_pausing, true);
        _pause_waitable.infinite_wait();
    }
    while (this->is_worker_started())
    {
        now = _clock.now();
        if ((ret = this->step()) == false)
            break ;
        after = _clock.now();
        ScopedModifier m(_pausing, true);
        _pause_waitable.wait_for(_sleep_time - (after - now));
        if (_pause)
            _pause_waitable.infinite_wait();
    }
    return ret;
}

void    StepWorker::resume_worker()
{
    _pause = false;
    while (_pausing.load() == true)
        _pause_waitable.notify_all();
}

void    StepWorker::pause_worker()
{
    _pause = true;
}

bool    StepWorker::step()
{
    return Worker::run();
}

bool    StepWorker::on_worker_start()
{
    if (_sleep_time == 0)
    {
        SIHD_LOG(error, "StepWorker: no frequency configured");
        return false;
    }
    return true;
}

bool    StepWorker::on_worker_stop()
{
    this->resume_worker();
    return true;
}

}