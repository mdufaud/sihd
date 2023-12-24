#include <sihd/util/Logger.hpp>
#include <sihd/util/ScopedModifier.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Worker::Worker(): _started(false), _running(false), _detach(false) {}

Worker::Worker(IRunnable *runnable): Worker()
{
    this->set_runnable(runnable);
}

Worker::Worker(std::function<bool()> method): Worker()
{
    this->set_method(method);
}

Worker::~Worker()
{
    this->stop_worker();
}

void Worker::set_worker_detach(bool active)
{
    _detach = active;
}

void Worker::set_method(std::function<bool()> method)
{
    _run_method = std::move(method);
}

void Worker::set_runnable(IRunnable *runnable)
{
    _run_method = [runnable] {
        return runnable->run();
    };
}

bool Worker::start_worker(std::string_view name)
{
    if (_started.exchange(true) == true)
        return true;

    if (!_run_method)
    {
        _started = false;
        SIHD_LOG_ERROR("Worker: cannot start worker '{}': nothing to run", name);
        return false;
    }

    bool ret = this->on_worker_start();

    if (ret)
    {
        _worker_thread_name = name;
        _worker_thread = std::thread(&Worker::_prepare_run, this);
        if (_detach)
            _worker_thread.detach();
    }

    _started = ret;
    return ret;
}

bool Worker::start_sync_worker(std::string_view name)
{
    if (_synchro.total_sync() > 0)
        return true;
    _synchro.init_sync(2);
    bool ret = this->start_worker(name);
    if (ret)
        _synchro.sync();
    _synchro.reset();
    return ret;
}

bool Worker::_prepare_run()
{
    ScopedModifier m(_running, true);
    thread::set_name(_worker_thread_name);
    if (_synchro.total_sync() > 0)
        _synchro.sync();
    bool ret = this->run();
    return ret;
}

bool Worker::run()
{
    return _run_method();
}

bool Worker::stop_worker()
{
    if (_started.exchange(false) == false)
        return true;
    bool ret = this->on_worker_stop();
    if (_worker_thread.joinable())
        _worker_thread.join();
    return ret;
}

bool Worker::on_worker_start()
{
    return true;
}

bool Worker::on_worker_stop()
{
    return true;
}

} // namespace sihd::util