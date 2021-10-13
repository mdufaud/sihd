#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

StepWorker::StepWorker(IRunnable *runnable): Worker(runnable)
{
    _sleep_time = 0;
    this->add_conf("frequency", &StepWorker::set_frequency);
}

StepWorker::~StepWorker()
{
}

bool    StepWorker::set_frequency(double frequency)
{
    if (frequency <= 0.0)
    {
        LOG(error, "StepWorker: frequency " << frequency << " cannot be negative");
        return false;
    }
    _sleep_time = time::freq(frequency);
    return true;
}

bool    StepWorker::run()
{
    Thread::set_name(_worker_thread_name);
    std::time_t now = 0;
    std::time_t after = 0;
    bool ret = true;
    while (this->is_running())
    {
        now = _clock.now();
        if ((ret = this->step()) == false)
            break ;
        after = _clock.now();
        _waitable.wait_for(_sleep_time - (after - now));
    }
    return ret;
}

bool    StepWorker::step()
{
    return _runnable_ptr->run();
}

bool    StepWorker::on_worker_start()
{
    if (_sleep_time == 0)
    {
        LOG(error, "StepWorker: no frequency configured");
        return false;
    }
    return true;
}

bool    StepWorker::on_worker_stop()
{
    _waitable.notify_all();
    return true;
}

}