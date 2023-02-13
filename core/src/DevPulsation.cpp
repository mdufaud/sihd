#include <sihd/core/DevPulsation.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Task.hpp>

# define CHANNEL_HEART "heartbeat"
# define CHANNEL_ACTIVATE "activate"

namespace sihd::core
{

using namespace sihd::util;

SIHD_UTIL_REGISTER_FACTORY(DevPulsation)

SIHD_LOGGER;

DevPulsation::DevPulsation(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _frequency(0.0),
    _scheduler("scheduler", this),
    _channel_heartbeat_ptr(nullptr),
    _channel_activate_ptr(nullptr)
{
    _scheduler.set_parent_ownership(false);
    this->add_conf("frequency", &DevPulsation::set_frequency);
}

DevPulsation::~DevPulsation()
{
    _scheduler.stop();
}

bool    DevPulsation::is_running() const
{
    return _running;
}

bool    DevPulsation::set_frequency(double freq)
{
    if (freq < 0)
    {
        SIHD_LOG(error, "DevPulsation: impossible frequency: {} hz", freq);
        return false;
    }
    _frequency = freq;
    return true;
}

void    DevPulsation::handle(sihd::core::Channel *c)
{
    if (c == _channel_activate_ptr)
    {
        if (c->read<bool>(0) == true)
            _scheduler.resume();
        else
            _scheduler.pause();
    }
}

bool    DevPulsation::run()
{
    {
        std::lock_guard l(_mutex);
        if (_running == false)
            return false;
    }
    _channel_heartbeat_ptr->write<uint32_t>(0, _beats);
    ++_beats;
    return true;
}

bool    DevPulsation::on_init()
{
    this->add_unlinked_channel(CHANNEL_HEART, sihd::util::TYPE_UINT, 1);
    this->add_unlinked_channel(CHANNEL_ACTIVATE, sihd::util::TYPE_BOOL, 1);
    return true;
}

bool    DevPulsation::on_start()
{
    if (_frequency == 0)
    {
        SIHD_LOG(error, "DevPulsation: cannot start without a frequency configured");
        return false;
    }
    if (!this->get_channel(CHANNEL_HEART, &_channel_heartbeat_ptr))
        return false;
    _beats = _channel_heartbeat_ptr->read<uint32_t>(0) + 1;
    if (!this->get_channel(CHANNEL_ACTIVATE, &_channel_activate_ptr))
        return false;
    this->observe_channel(_channel_activate_ptr);
    if (_channel_activate_ptr->read<bool>(0) == false)
        _scheduler.pause();
    _scheduler.add_task(new sihd::util::Task(this, 0, sihd::util::time::freq(_frequency)));
    if (_scheduler.start() == false)
    {
        SIHD_LOG(error, "DevPulsation: could not start scheduler");
        return false;
    }
    _running = true;
    return true;
}

bool    DevPulsation::on_stop()
{
    {
        std::lock_guard l(_mutex);
        _running = false;
    }
    if (_scheduler.stop() == false)
    {
        SIHD_LOG(error, "DevPulsation: could not stop scheduler");
        return false;
    }
    return true;
}

bool    DevPulsation::on_reset()
{
    _scheduler.clear_tasks();
    return true;
}

}
