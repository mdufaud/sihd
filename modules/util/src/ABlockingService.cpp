#include <sihd/util/ABlockingService.hpp>

namespace sihd::util
{

ABlockingService::ABlockingService(): _running(false), _ready(false), _wait_stop(true) {}

void ABlockingService::set_service_wait_stop(bool active)
{
    _wait_stop = active;
}

void ABlockingService::service_set_ready()
{
    {
        auto l = _waitable.guard();
        this->_ready = true;
    }
    _waitable.notify_all();
}

bool ABlockingService::is_ready() const
{
    return _ready;
}

bool ABlockingService::is_running() const
{
    return _running;
}

bool ABlockingService::wait_ready(Duration timeout) const
{
    return _waitable.wait_for(timeout, [this]() { return this->is_ready(); });
}

bool ABlockingService::do_start()
{
    std::lock_guard l(_running_mutex);
    {
        auto l = _waitable.guard();
        this->_ready = false;
    }
    _running_thread = std::this_thread::get_id();
    _running = true;
    const bool ret = this->on_start();
    _running = false;
    _running_thread = std::thread::id();
    return ret;
}

bool ABlockingService::do_stop()
{
    bool ret = this->on_stop();
    {
        auto l = _waitable.guard();
        this->_ready = false;
    }
    // stopped from its own on_start (an observer of the service): it ends when that call returns
    if (_wait_stop && _running_thread.load() != std::this_thread::get_id())
        this->service_wait_stop();
    return ret;
}

void ABlockingService::service_wait_stop() const
{
    std::lock_guard l(_running_mutex);
}

} // namespace sihd::util
