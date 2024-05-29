#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

StepWorker::StepWorker(): Worker(), _pause(false), _sleep_time(0) {}

StepWorker::StepWorker(IRunnable *runnable): StepWorker()
{
    this->set_runnable(runnable);
}

StepWorker::StepWorker(std::function<bool()> method): StepWorker()
{
    this->set_method(method);
}

StepWorker::~StepWorker()
{
    this->stop_worker();
}

void StepWorker::set_callback_setup(Callback && fun)
{
    _callback_setup = std::move(fun);
}

void StepWorker::set_callback_teardown(Callback && fun)
{
    _callback_teardown = std::move(fun);
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

    if (_callback_setup)
        _callback_setup();

    _pause_waitable.wait([this] { return _pause == false; });
    while (this->is_worker_started())
    {
        if ((ret = this->step()) == false)
            break;
        _pause_waitable.wait_for(_sleep_time, [this] { return this->is_worker_started() == false; });
        _pause_waitable.wait([this] { return _pause == false; });
    }

    if (_callback_teardown)
        _callback_teardown();

    return ret;
}

void StepWorker::resume_worker()
{
    auto l = _pause_waitable.guard();
    _pause = false;
    _pause_waitable.notify_all();
}

void StepWorker::pause_worker()
{
    auto l = _pause_waitable.guard();
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
