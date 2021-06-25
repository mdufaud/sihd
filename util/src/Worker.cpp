#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Worker::Worker(const std::string & name, Node *parent):
    Node(name, parent), _running(false)
{
    this->add_conf("frequency", &Worker::set_frequency);
}

Worker::~Worker()
{
    this->stop_thread();
}

bool    Worker::set_frequency(double frequency)
{
    if (frequency <= 0.0)
    {
        LOG(error, "Worker: frequency " << frequency << " ");
        return false;
    }
    _sleep_time = time::freq(frequency);
    return true;
}

bool    Worker::set_method(std::function<bool(time_t)> method)
{
    _step_method = std::move(method);
    return true;
}

bool    Worker::run()
{
    thread::set_name(this->get_name());
    std::time_t delta = 0;
    std::time_t now = 0;
    std::time_t after = 0;
    bool ret = true;
    while (_running)
    {
        now = _clock.now();
        // branchless
        delta = (now - after) * (after != 0);
        if ((ret = this->step(delta)) == false)
            break ;
        after = _clock.now();
        _waitable.wait_for(_sleep_time - (after - now));
    }
    return ret;
}

bool    Worker::step(std::time_t delta)
{
    return _step_method(delta);
}

bool    Worker::start_thread()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_running == true)
            return false;
        _running = true;
    }
    _worker_thread = std::thread(&Worker::run, this);
    return true;
}

bool    Worker::stop_thread()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_running == false)
            return false;
        _running = false;
    }
    _waitable.notify();
    if (_worker_thread.joinable())
        _worker_thread.join();
    return true;
}

}