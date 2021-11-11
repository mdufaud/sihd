#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::core
{

LOGGER;

ChannelWaiter::ChannelWaiter(Channel *c): ACoreObject("channel_waiter", nullptr), _channel(nullptr)
{
    if (c != nullptr)
        this->set_channel(c);
}

ChannelWaiter::ChannelWaiter(const std::string & name, sihd::util::Node *parent):
    ACoreObject(name, parent), _channel(nullptr)
{
}

ChannelWaiter::~ChannelWaiter()
{
    this->clear_channel();
}

bool    ChannelWaiter::do_stop()
{
    this->clear_channel();
    return true;
}

void    ChannelWaiter::clear_channel()
{
    _waitable.notify_all();
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
    // waitable returns true when timed out
    return _waitable.wait_loop(nano, notifications) == false;
}

void    ChannelWaiter::handle(Channel *channel)
{
    (void)channel;
    _waitable.notify(1);
}

}