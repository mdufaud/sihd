#include <sihd/util/AStepWorkerService.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

AStepWorkerService::AStepWorkerService(std::string_view thread_name): AThreadedService(thread_name)
{
    this->set_service_nb_thread(1);

    _step_worker.set_runnable(this);
    _step_worker.set_callback_setup([this] {
        this->on_work_setup();
        this->notify_service_thread_started();
    });
    _step_worker.set_callback_teardown([this] { this->on_work_teardown(); });
}

AStepWorkerService::~AStepWorkerService() = default;

bool AStepWorkerService::set_step_frequency(double frequency)
{
    return _step_worker.set_frequency(frequency);
}

bool AStepWorkerService::run()
{
    return this->on_work_start();
}

bool AStepWorkerService::on_start()
{
    return _step_worker.start_worker(this->thread_name());
}

bool AStepWorkerService::on_stop()
{
    this->on_work_stop();
    return _step_worker.stop_worker();
}

bool AStepWorkerService::is_running() const
{
    return _step_worker.is_worker_running();
}

} // namespace sihd::util
