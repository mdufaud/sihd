#include <sihd/util/BlockingService.hpp>

namespace sihd::util
{

BlockingService::BlockingService(): _running(false), _wait_stop(true) {}

void BlockingService::set_service_wait_stop(bool active)
{
    _wait_stop = active;
}

bool BlockingService::is_running() const
{
    return _running;
}

bool BlockingService::do_start()
{
    std::lock_guard l(_running_mutex);
    _running = true;
    bool ret = this->on_start();
    _running = false;
    return ret;
}

bool BlockingService::do_stop()
{
    bool ret = this->on_stop();
    if (_wait_stop)
        this->service_wait_stop();
    return ret;
}

void BlockingService::service_wait_stop() const
{
    std::lock_guard l(_running_mutex);
}

} // namespace sihd::util
