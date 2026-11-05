#include <sihd/core/Core.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(Core)

SIHD_LOGGER;

Core::Core(const std::string & name, Node *parent): sihd::core::Device(name, parent) {}

bool Core::is_running() const
{
    return this->device_state() == ServiceController::Running;
}

bool Core::on_init()
{
    return true;
}

bool Core::on_start()
{
    return true;
}

bool Core::on_stop()
{
    return true;
}

bool Core::on_reset()
{
    return true;
}

} // namespace sihd::core
