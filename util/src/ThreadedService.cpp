#include <sihd/util/ThreadedService.hpp>

namespace sihd::util
{

ThreadedService::ThreadedService(): _number_of_threads(1), _start_synchronised(false) {}

ThreadedService::~ThreadedService() {}

void ThreadedService::set_service_nb_thread(uint8_t n)
{
    _number_of_threads = n;
}

void ThreadedService::set_start_synchronised(bool active)
{
    _start_synchronised = active;
}

bool ThreadedService::do_start()
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

bool ThreadedService::do_stop()
{
    return this->on_stop();
}

void ThreadedService::notify_service_thread_started()
{
    if (_synchro.total_sync() > 0)
        _synchro.sync();
}

} // namespace sihd::util
