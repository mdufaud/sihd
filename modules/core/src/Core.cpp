#include <sihd/core/Core.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

namespace sihd::core
{

SIHD_REGISTER_FACTORY(Core)

SIHD_LOGGER;

Core::Core(const std::string & name, sihd::util::Node *parent): sihd::core::Device(name, parent)
{
    _running = false;
    _is_reset = true;
}

Core::~Core()
{
    if (_running)
        this->stop();
    if (_is_reset == false)
        this->reset();
}

bool Core::on_init()
{
    _is_reset = false;
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
    _is_reset = true;
    return true;
}

} // namespace sihd::core