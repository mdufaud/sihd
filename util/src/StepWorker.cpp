#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

StepWorker::StepWorker(IRunnable *runnable): Worker(runnable), _pause(false), _sleep_time(0)
{
    this->add_conf("frequency", &StepWorker::set_frequency);
}

StepWorker::~StepWorker()
{
    this->stop_worker();
}

bool StepWorker::set_frequency(double frequency)
{
    if (frequency <= 0.0)
    {
        SIHD_LOG(error, "StepWorker: frequency {} cannot be negative", frequency);
        return false;
    }
    _sleep_time = time::freq(frequency);
    return true;
}

double StepWorker::frequency() const
{
    return time::to_hz(_sleep_time);
}

bool StepWorker::run()
{
    bool ret = true;
    time_t now = 0;
    time_t after = 0;
    _pause_waitable.wait([this] { return _pause == false; });
    while (this->is_worker_started())
    {
        now = _clock.now();
        if ((ret = this->step()) == false)
            break;
        after = _clock.now();
        _pause_waitable.wait_for(_sleep_time - (after - now));
        _pause_waitable.wait([this] { return _pause == false; });
    }
    return ret;
}

void StepWorker::resume_worker()
{
    _pause = false;
    _pause_waitable.notify_all();
}

void StepWorker::pause_worker()
{
    _pause = true;
}

bool StepWorker::step()
{
    return Worker::run();
}

bool StepWorker::on_worker_start()
{
    if (_sleep_time == 0)
    {
        SIHD_LOG(error, "StepWorker: no frequency configured");
        return false;
    }
    return true;
}

bool StepWorker::on_worker_stop()
{
    this->resume_worker();
    return true;
}

} // namespace sihd::util
