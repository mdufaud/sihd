#include <sihd/core/DevPulsation.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Task.hpp>

#define CHANNEL_HEART "heartbeat"
#define CHANNEL_ACTIVATE "activate"

namespace sihd::core
{

using namespace sihd::util;

SIHD_UTIL_REGISTER_FACTORY(DevPulsation)

SIHD_LOGGER;

DevPulsation::DevPulsation(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _step_worker(this),
    _channel_heartbeat_ptr(nullptr),
    _channel_activate_ptr(nullptr)
{
    this->add_conf("frequency", &DevPulsation::set_frequency);
}

DevPulsation::~DevPulsation()
{
    _step_worker.stop_worker();
}

bool DevPulsation::is_running() const
{
    return _step_worker.is_worker_running();
}

bool DevPulsation::set_frequency(double freq)
{
    return _step_worker.set_frequency(freq);
}

void DevPulsation::handle(sihd::core::Channel *c)
{
    if (c == _channel_activate_ptr)
    {
        if (c->read<bool>(0) == true)
        {
            _step_worker.resume_worker();
        }
        else
        {
            _step_worker.pause_worker();
        }
    }
}

bool DevPulsation::run()
{
    ++_beats;
    _channel_heartbeat_ptr->write<uint32_t>(0, _beats);
    return true;
}

bool DevPulsation::on_init()
{
    this->add_unlinked_channel(CHANNEL_HEART, sihd::util::TYPE_UINT, 1);
    this->add_unlinked_channel(CHANNEL_ACTIVATE, sihd::util::TYPE_BOOL, 1);
    return true;
}

bool DevPulsation::on_start()
{
    if (_step_worker.frequency() == 0.0)
    {
        SIHD_LOG(error, "DevPulsation: cannot start without a frequency configured");
        return false;
    }
    if (!this->get_channel(CHANNEL_HEART, &_channel_heartbeat_ptr))
        return false;
    _beats = _channel_heartbeat_ptr->read<uint32_t>(0);

    if (!this->get_channel(CHANNEL_ACTIVATE, &_channel_activate_ptr))
        return false;
    this->observe_channel(_channel_activate_ptr);
    if (_channel_activate_ptr->read<bool>(0) == false)
        _step_worker.pause_worker();

    if (_step_worker.start_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevPulsation: could not start");
        return false;
    }
    return true;
}

bool DevPulsation::on_stop()
{
    if (_step_worker.stop_worker() == false)
    {
        SIHD_LOG(error, "DevPulsation: could not stop");
        return false;
    }
    return true;
}

bool DevPulsation::on_reset()
{
    return true;
}

} // namespace sihd::core
