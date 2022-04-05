#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

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

bool    Worker::start_worker(const std::string & name)
{
    if (_runnable_ptr == nullptr)
    {
        SIHD_LOG_ERROR("Worker: cannot start worker '%s': nothing to run", name.c_str());
        return false;
    }
    {
        std::lock_guard lock(_worker_mutex);
        if (_started == true)
            return true;
        _started = true;
    }
    bool ret = this->on_worker_start();
    if (ret)
    {
        _worker_thread_name = name;
        _worker_thread = std::thread(&Worker::run, this);
    }
    return ret;
}

bool    Worker::start_sync_worker(const std::string & name)
{
    std::lock_guard lock(_worker_sync_mutex);
    bool ret = this->start_worker(name);
    while (ret && _running == false)
        _running_waitable.wait_for(time::micro(100));
    return ret;
}

bool    Worker::run()
{
    // unlocks start_sync_worker
    this->_worker_set_running(true);
    // start only when start_sync_worker ends
    std::lock_guard lock(_worker_sync_mutex);

    Thread::set_name(_worker_thread_name);
    bool ret = _runnable_ptr->run();
    this->_worker_set_running(false);
    return ret;
}

bool    Worker::stop_worker()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_started == false)
            return true;
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
    {
        std::lock_guard lock(_worker_mutex);
        _running = active;
        _running_waitable.notify_all();
    }
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