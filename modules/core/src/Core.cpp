#include <sihd/core/Core.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(Core)

SIHD_LOGGER;

Core::Core(const std::string & name, Node *parent): sihd::core::Device(name, parent)
{
    _running = false;
}

Core::~Core()
{
    if (_running)
        this->stop();
    // a failed start or stop leaves a state that refuses a reset
    if (this->device_state() == ServiceController::Stopped)
        this->reset();
}

bool Core::is_running() const
{
    return _running;
}

bool Core::on_init()
{
    return true;
}

bool Core::on_start()
{
    _running = true;
    return true;
}

bool Core::on_stop()
{
    _running = false;
    return true;
}

bool Core::on_reset()
{
    return true;
}

} // namespace sihd::core