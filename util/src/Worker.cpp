#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Worker::Worker(IRunnable *runnable): _runnable_ptr(nullptr), _started(false), _running(false), _detach(false)
{
    this->set_runnable(runnable);
}

Worker::~Worker()
{
    this->stop_worker();
}

void    Worker::set_worker_detach(bool active)
{
    _detach = active;
}

void    Worker::set_runnable(IRunnable *runnable)
{
    _runnable_ptr = runnable;
}

bool    Worker::start_worker(std::string_view name)
{
    if (_runnable_ptr == nullptr)
    {
        SIHD_LOG_ERROR("Worker: cannot start worker '%s': nothing to run", name.data());
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
        _worker_thread = std::thread(&Worker::_prepare_run, this);
        if (_detach)
            _worker_thread.detach();
    }
    return ret;
}

bool    Worker::start_sync_worker(std::string_view name)
{
    _synchro.init_sync(2);
    bool ret = this->start_worker(name);
    if (ret)
        _synchro.sync();
    _synchro.reset();
    return ret;
}

bool    Worker::_prepare_run()
{
    this->_worker_set_running(true);
    Thread::set_name(_worker_thread_name);
    if (_synchro.to_sync() > 0)
        _synchro.sync();
    bool ret = this->run();
    this->_worker_set_running(false);
    return ret;
}

bool    Worker::run()
{
    return _runnable_ptr->run();
}

bool    Worker::stop_worker()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_started == false)
            return true;
        _started = false;
    }
    bool ret = this->on_worker_stop();
    if (_worker_thread.joinable())
        _worker_thread.join();
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