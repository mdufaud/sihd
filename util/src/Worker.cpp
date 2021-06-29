#include <sihd/util/Worker.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Worker::Worker(): _running(false)
{
}

Worker::~Worker()
{
    this->stop_worker();
}

bool    Worker::set_method(std::function<bool()> method)
{
    _worker_run_method = std::move(method);
    return true;
}

bool    Worker::run()
{
    thread::set_name(_worker_thread_name);
    return _worker_run_method();
}

bool    Worker::start_worker(const std::string & name)
{
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