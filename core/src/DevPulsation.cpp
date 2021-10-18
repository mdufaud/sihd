#include <sihd/core/DevPulsation.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

# define CHANNEL_HEART "heartbeat"
# define CHANNEL_ACTIVATE "activate"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevPulsation)

LOGGER;

DevPulsation::DevPulsation(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false), _channel_heartbeat_ptr(nullptr)
{
    this->add_conf("frequency", &DevPulsation::set_frequency);
}

DevPulsation::~DevPulsation()
{
}

bool    DevPulsation::is_running() const
{
    return _running;
}

bool    DevPulsation::set_frequency(double freq)
{
    if (freq < 0)
    {
        LOG(error, "DevPulsation: impossible frequency: " << freq << " hz");
        return false;
    }
    _frequency = freq;
    return true;
}

void    DevPulsation::observable_changed(sihd::core::Channel *c)
{
    if (c == _channel_activate_ptr)
    {
        if (c->read<bool>(0) == true)
            _scheduler_ptr->resume();
        else
            _scheduler_ptr->pause();
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
    _scheduler_ptr = new sihd::util::Scheduler("scheduler", this);
    this->add_unlinked_channel(CHANNEL_HEART, sihd::util::DUINT, 1);
    this->add_unlinked_channel(CHANNEL_ACTIVATE, sihd::util::DBOOL, 1);
    return true;
}

bool    DevPulsation::on_start()
{
    if (_frequency == 0)
    {
        LOG(error, "DevPulsation: cannot start without a frequency configured");
        return false;
    }
    if (!this->get_channel(CHANNEL_HEART, &_channel_heartbeat_ptr))
        return false;
    _beats = _channel_heartbeat_ptr->read<uint32_t>(0) + 1;
    if (!this->get_channel(CHANNEL_ACTIVATE, &_channel_activate_ptr))
        return false;
    this->observe_channel(_channel_activate_ptr);
    if (_channel_activate_ptr->read<bool>(0) == false)
        _scheduler_ptr->pause();
    if (_scheduler_ptr->start() == false)
    {
        LOG(error, "DevPulsation: could not start scheduler");
        return false;
    }
    _scheduler_ptr->add_task(new sihd::util::Task(this, 0, sihd::util::time::freq(_frequency)));
    _running = true;
    return true;
}

bool    DevPulsation::on_stop()
{
    {
        std::lock_guard l(_mutex);
        _running = false;
    }
    if (_scheduler_ptr->stop() == false)
    {
        LOG(error, "DevPulsation: could not stop scheduler");
        return false;
    }
    return true;
}

bool    DevPulsation::on_reset()
{
    _scheduler_ptr = nullptr;
    return true;
}

}