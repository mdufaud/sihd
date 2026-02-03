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

bool ABlockingService::wait_ready(Timestamp timeout) const
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
    _running = true;
    const bool ret = this->on_start();
    _running = false;
    return ret;
}

bool ABlockingService::do_stop()
{
    bool ret = this->on_stop();
    {
        auto l = _waitable.guard();
        this->_ready = false;
    }
    if (_wait_stop)
        this->service_wait_stop();
    return ret;
}

void ABlockingService::service_wait_stop() const
{
    std::lock_guard l(_running_mutex);
}

} // namespace sihd::util
