#include <sihd/util/AThreadedService.hpp>

namespace sihd::util
{

AThreadedService::AThreadedService(std::string_view thread_name):
    _thread_name(thread_name),
    _number_of_threads(1),
    _start_synchronised(false)
{
}

AThreadedService::~AThreadedService() {}

void AThreadedService::set_service_nb_thread(uint8_t n)
{
    _number_of_threads = n;
}

void AThreadedService::set_start_synchronised(bool active)
{
    _start_synchronised = active;
}

bool AThreadedService::do_start()
{
    bool ret;

    if (_start_synchronised && _number_of_threads > 0)
    {
        _synchro.init_sync(_number_of_threads + 1);
        ret = this->on_start();
        if (ret)
            _synchro.sync();
        _synchro.reset();
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
    if (_synchro.total_sync() > 0)
        _synchro.sync();
}

const std::string & AThreadedService::thread_name() const
{
    return _thread_name;
}

} // namespace sihd::util
