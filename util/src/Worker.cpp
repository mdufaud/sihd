#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Worker::Worker(IRunnable *runnable): _runnable_ptr(nullptr), _running(false)
{
    this->set_runnable(runnable);
}

Worker::~Worker()
{
    this->stop_worker();
    if (_runnable_ptr != nullptr)
        delete _runnable_ptr;
}

void    Worker::set_runnable(IRunnable *runnable)
{
    if (_runnable_ptr != nullptr)
        delete _runnable_ptr;
    _runnable_ptr = runnable;
}

bool    Worker::run()
{
    Thread::set_name(_worker_thread_name);
    return _runnable_ptr->run();
}

bool    Worker::start_worker(const std::string & name)
{
    if (_runnable_ptr == nullptr)
    {
        LOG_ERROR("Worker: cannot start worker '%s': nothing to run", name.c_str());
        return false;
    }
    {
        std::lock_guard lock(_worker_mutex);
        if (_running == true)
            return false;
        _running = true;
    }
    _worker_thread_name = name;
    bool ret = this->on_worker_start();
    if (ret)
        _worker_thread = std::thread(&Worker::run, this);
    else
    {
        std::lock_guard lock(_worker_mutex);
        _running = false;
    }
    return ret;
}

bool    Worker::stop_worker()
{
    {
        std::lock_guard lock(_worker_mutex);
        if (_running == false)
            return false;
        _running = false;
    }
    if (_worker_thread.joinable())
    {
        bool ret = this->on_worker_stop();
        _worker_thread.join();
        return ret;
    }
    return true;
}

bool    Worker::on_worker_start()
{
    return true;
}

bool    Worker::on_worker_stop()
{
    return true;
}

}