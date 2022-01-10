#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

LOGGER;

Worker::Worker(IRunnable *runnable): _runnable_ptr(nullptr), _started(false), _running(false)
{
    this->set_runnable(runnable);
}

Worker::~Worker()
{
    this->stop_worker();
}

void    Worker::set_runnable(IRunnable *runnable)
{
    _runnable_ptr = runnable;
}

bool    Worker::run()
{
    Thread::set_name(_worker_thread_name);
    this->_worker_set_running(true);
    bool ret = _runnable_ptr->run();
    this->_worker_set_running(false);
    return ret;
}

bool    Worker::wait_worker(time_t nano_timeout)
{
    std::lock_guard lock(_worker_mutex);
    {
        if (_started == false || _running)
            return _running;
    }
    if (nano_timeout >= 0)
        _running_waitable.wait_for(nano_timeout);
    else
        _running_waitable.infinite_wait();
    return _running;
}

bool    Worker::start_worker(const std::string & name)
{
    if (_runnable_ptr == nullptr)
    {
        LOG_ERROR("Worker: cannot start worker '%s': nothing to run", name.c_str());
        _running_waitable.notify_all();
        return false;
    }
    {
        std::lock_guard lock(_worker_mutex);
        if (_started == true)
        {
            _running_waitable.notify_all();
            return false;
        }
        _started = true;
    }
    bool ret = this->on_worker_start();
    if (ret)
    {
        _worker_thread_name = name;
        _worker_thread = std::thread(&Worker::run, this);
    }
    else
        _running_waitable.notify_all();
    return ret;
}

bool    Worker::stop_worker()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_started == false)
            return false;
        _started = false;
    }
    bool ret = true;
    if (_worker_thread.joinable())
    {
        ret = this->on_worker_stop();
        _worker_thread.join();
    }
    return ret;
}

bool    Worker::on_worker_start()
{
    return true;
}

bool    Worker::on_worker_stop()
{
    return true;
}

void    Worker::_worker_set_running(bool active)
{
    _running = active;
    _running_waitable.notify_all();
}

std::string &   Worker::_worker_get_name()    
{
    return _worker_thread_name;
}

IRunnable   *Worker::_worker_get_runnable()
{
    return _runnable_ptr;
}

}