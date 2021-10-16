#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::core
{

LOGGER;

ChannelWaiter::ChannelWaiter(Channel *c): _channel(nullptr)
{
    if (c != nullptr)
        this->set_channel(c);
}

ChannelWaiter::~ChannelWaiter()
{
    this->clear_channel();
}

void    ChannelWaiter::clear_channel()
{
    if (_channel != nullptr)
        _channel->remove_observer(this);
    _channel = nullptr;
}

bool    ChannelWaiter::set_channel(Channel *channel)
{
    if (channel == nullptr)
    {
        LOG(error, "ChannelWaiter: no channel to set");
        return false;
    }
    if (_channel == channel)
        return true;
    this->clear_channel();
    if (channel->add_observer(this) == false)
    {
        LOG(error, "ChannelWaiter: could not add observer to channel: " << channel->get_full_name());
        return false;
    }
    _channel = channel;
    return true;
}

bool    ChannelWaiter::wait_for(time_t nano, uint32_t notifications)
{
    if (_channel == nullptr)
        return true;
    return _waitable.wait_loop(nano, notifications);
}

void    ChannelWaiter::observable_changed(Channel *channel)
{
    (void)channel;
    _waitable.notify(1);
}

}