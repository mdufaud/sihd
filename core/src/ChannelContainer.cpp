#include <sihd/core/ChannelContainer.hpp>

namespace sihd::core
{

ChannelContainer::ChannelContainer(const std::string & name, Node *parent): Node(name, parent)
{
}

ChannelContainer::~ChannelContainer()
{
}

Channel     *ChannelContainer::get_channel(const std::string & name)
{
    Named *child = this->get_child(name);
    if (child)
        return dynamic_cast<Channel *>(child);
    return nullptr;
}

bool    ChannelContainer::get_channel(const std::string & name, Channel **to_fill)
{
    Channel *c = this->get_channel(name);
    if (c == nullptr)
    {
        LOG_ERROR("ChannelContainer: '%s' no such channel '%s'", this->get_full_name().c_str(), name.c_str());
        return false;
    }
    *to_fill = c;
    return true;
}

Channel *ChannelContainer::add_channel(const std::string & name, const std::string & type, size_t size)
{
    Channel *c = new Channel(name, type, size);
    if (c->set_parent(this) == false)
    {
        LOG_ERROR("ChannelContainer: '%s' cannot add channel '%s'", this->get_full_name().c_str(), name.c_str());
        delete c;
        return nullptr;
    }
    return c;
}

Channel *ChannelContainer::add_unlinked_channel(const std::string & name, const std::string & type, size_t size)
{
    if (this->is_link(name))
    {
        _channels_link[name] = {sihd::util::Datatype::string_to_datatype(type), size};
        return nullptr;
    }
    return this->add_channel(name, type, size);
}

bool    ChannelContainer::_check_link(const std::string & name, Named *child)
{
    Channel *chan = dynamic_cast<Channel *>(child);
    if (chan == nullptr)
        return true;
    if (_channels_link.find(name) == _channels_link.end())
    {
        return true;
    }
    bool ret = true;
    ChannelConfiguration conf = _channels_link[name];
    if (conf.type != chan->arr()->data_type())
    {
        LOG_ERROR("ChannelContainer: '%s' channel link size not same type '%s': '%s' != '%s'",
                    this->get_full_name().c_str(), name.c_str(),
                    sihd::util::Datatype::datatype_to_string(conf.type),
                    chan->arr()->data_type_to_string());
        ret = false;
    }
    if (conf.size != chan->arr()->data_size())
    {
        LOG_ERROR("ChannelContainer: '%s' channel link size not equal '%s': '%lu' != '%lu'",
                    this->get_full_name().c_str(), name.c_str(),
                    conf.size, chan->arr()->data_size());
        ret = false;
    }
    return ret;
}

bool    ChannelContainer::observe_channel(const std::string & channel_name)
{
    Channel *c = this->get_channel(channel_name);
    if (c != nullptr)
        return this->observe_channel(c);
    LOG_ERROR("ChannelContainer: '%s' cannot find channel '%s' to observe",
            this->get_full_name().c_str(), channel_name.c_str());
    return false;
}

bool    ChannelContainer::observe_channel(Channel *c)
{
    if (c != nullptr)
    {
        // false if already added
        if (c->add_observer(this))
            _observed_channels.push_back(c);
        return true;
    }
    return false;
}

void    ChannelContainer::remove_channels_observation()
{
    for (Channel *c: _observed_channels)
    {
        c->remove_observer(this);
    }
    _observed_channels.clear();
}

}