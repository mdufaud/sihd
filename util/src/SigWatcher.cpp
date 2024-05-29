#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/thread.hpp>

#include <fmt/format.h>

namespace sihd::util
{

SigWatcher::SigWatcher(const std::string & name, Node *parent): Named(name, parent)
{
    this->set_service_nb_thread(1);

    _step_worker.set_runnable(this);
    _step_worker.set_frequency(1.0);
    _step_worker.set_callback_setup([this] { this->notify_service_thread_started(); });

    this->add_conf("polling_frequency", &SigWatcher::set_polling_frequency);
}

SigWatcher::~SigWatcher()
{
    if (this->is_running())
        this->stop();
}

bool SigWatcher::set_polling_frequency(double hz)
{
    return _step_worker.set_frequency(hz);
}

bool SigWatcher::on_start()
{
    return _step_worker.start_worker(this->name());
}

bool SigWatcher::on_stop()
{
    return _step_worker.stop_worker();
}

bool SigWatcher::is_running() const
{
    return _step_worker.is_worker_running();
}

bool SigWatcher::add_signal(int sig)
{
    std::lock_guard l(_mutex);
    auto & sig_controller = _sig_controllers.emplace_back();

    bool success = sig_controller.sig_handler.handle(sig);
    if (success)
    {
        const auto status = signal::status(sig);
        success = status.has_value();
        if (success)
        {
            sig_controller.last_received = status->received;
        }
    }

    if (success)
        _signals.emplace_back(sig);
    else
        _sig_controllers.pop_back();

    return success;
}

bool SigWatcher::add_signals(const std::vector<int> & signals)
{
    return std::all_of(signals.begin(), signals.end(), [this](int sig) { return this->add_signal(sig); });
}

bool SigWatcher::rm_signal(int sig)
{
    std::lock_guard l(_mutex);
    return container::erase_if(_sig_controllers,
                               [sig](const auto & sig_controller) { return sig_controller.sig_handler.sig() == sig; })
           && container::erase(_signals, sig);
}

bool SigWatcher::rm_signals(const std::vector<int> & signals)
{
    return std::all_of(signals.begin(), signals.end(), [this](int sig) { return this->rm_signal(sig); });
}

bool SigWatcher::call_previous_handler(int sig)
{
    std::lock_guard l(_mutex);
    const auto it = container::find_if(_sig_controllers, [sig](const auto & sig_controller) {
        return sig_controller.sig_handler.sig() == sig;
    });
    const bool found = it != _sig_controllers.end();
    if (found)
        it->sig_handler.call_previous_handler();
    return found;
}

bool SigWatcher::run()
{
    {
        std::lock_guard l(_mutex);
        for (int signal : _signals)
        {
            auto it_sigcontroller = container::find_if(_sig_controllers, [signal](const auto & sig_controller) {
                return sig_controller.sig_handler.sig() == signal;
            });

            const auto status = signal::status(signal);
            if (status.has_value() && status->received != it_sigcontroller->last_received)
            {
                it_sigcontroller->last_received = status->received;
                _signals_to_handle.emplace_back(signal);
            }
        }
    }

    if (_signals_to_handle.size() > 0)
    {
        this->notify_observers(this);
        _signals_to_handle.clear();
    }

    return true;
}

} // namespace sihd::util
