#include <sihd/util/ABlockingService.hpp>

namespace sihd::util
{

ABlockingService::ABlockingService(): _running(false), _wait_stop(true) {}

void ABlockingService::set_service_wait_stop(bool active)
{
    _wait_stop = active;
}

bool ABlockingService::is_running() const
{
    return _running;
}

bool ABlockingService::do_start()
{
    std::lock_guard l(_running_mutex);
    _running = true;
    bool ret = this->on_start();
    _running = false;
    return ret;
}

bool ABlockingService::do_stop()
{
    bool ret = this->on_stop();
    if (_wait_stop)
        this->service_wait_stop();
    return ret;
}

void ABlockingService::service_wait_stop() const
{
    std::lock_guard l(_running_mutex);
}

} // namespace sihd::util
