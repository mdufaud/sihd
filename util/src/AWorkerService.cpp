#include <sihd/util/AWorkerService.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

AWorkerService::AWorkerService(std::string_view thread_name): AThreadedService(thread_name), _stop_requested(false)
{
    this->set_service_nb_thread(1);
    _worker.set_runnable(this);
}

AWorkerService::~AWorkerService() {}

bool AWorkerService::run()
{
    this->notify_service_thread_started();
    return this->on_work_start();
}

bool AWorkerService::on_start()
{
    _stop_requested = false;
    return _worker.start_worker(this->thread_name());
}

bool AWorkerService::is_work_stop_requested() const
{
    return _stop_requested;
}

bool AWorkerService::on_stop()
{
    _stop_requested = true;
    this->on_work_stop();
    return _worker.stop_worker();
}

bool AWorkerService::is_running() const
{
    return _worker.is_worker_running();
}

} // namespace sihd::util
