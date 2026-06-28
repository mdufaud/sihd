#include <sihd/util/AThreadedService.hpp>

namespace sihd::util
{

AThreadedService::AThreadedService(std::string_view thread_name):
    _thread_name(thread_name),
    _start_synchronised(false)
{
    this->set_service_nb_thread(1);
}

AThreadedService::~AThreadedService() = default;

void AThreadedService::set_service_nb_thread(uint8_t n)
{
    if (n > 0)
        _sync_ptr = std::make_unique<Synchronizer>(n + 1);
    else
        _sync_ptr.reset();
}

void AThreadedService::set_start_synchronised(bool active)
{
    _start_synchronised = active;
}

bool AThreadedService::do_start()
{
    bool ret;

    if (_start_synchronised && _sync_ptr)
    {
        ret = this->on_start();
        _sync_ptr->sync();
    }
    else
        ret = this->on_start();

    return ret;
}

bool AThreadedService::do_stop()
{
    return this->on_stop();
}

void AThreadedService::notify_service_thread_started()
{
    if (_sync_ptr)
        _sync_ptr->sync();
}

const std::string & AThreadedService::thread_name() const
{
    return _thread_name;
}

} // namespace sihd::util
