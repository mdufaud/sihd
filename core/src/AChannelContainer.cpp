#include <sihd/util/Logger.hpp>

#include <sihd/core/AChannelContainer.hpp>

namespace sihd::core
{

SIHD_LOGGER;

using namespace sihd::util;

AChannelContainer::AChannelContainer(const std::string & name, Node *parent): Node(name, parent) {}

AChannelContainer::~AChannelContainer() {}

Channel *AChannelContainer::find_channel(const std::string & path)
{
    return this->find<Channel>(path);
}

bool AChannelContainer::find_channel(const std::string & path, Channel **to_fill)
{
    Channel *c = this->find_channel(path);
    if (c == nullptr)
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' no such channel '{}'", this->full_name(), path);
        return false;
    }
    *to_fill = c;
    return true;
}

Channel *AChannelContainer::get_channel(const std::string & name)
{
    Named *child = this->get_child(name);
    if (child)
        return dynamic_cast<Channel *>(child);
    return nullptr;
}

bool AChannelContainer::get_channel(const std::string & name, Channel **to_fill)
{
    Channel *c = this->get_channel(name);
    if (c == nullptr)
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' no such channel '{}'", this->full_name(), name);
        return false;
    }
    *to_fill = c;
    return true;
}

Channel *AChannelContainer::add_unlinked_channel(const std::string & name,
                                                 sihd::util::Type type,
                                                 size_t size,
                                                 bool check_match)
{
    if (this->is_link(name))
    {
        _channels_link[name] = {type, size, check_match};
        return nullptr;
    }
    return this->add_channel(name, type, size);
}

Channel *AChannelContainer::add_unlinked_channel(const std::string & name,
                                                 std::string_view type,
                                                 size_t size,
                                                 bool check_match)
{
    return this->add_unlinked_channel(name, sihd::util::type::from_str(type), size, check_match);
}

Channel *AChannelContainer::add_channel(const std::string & name, sihd::util::Type type, size_t size)
{
    Channel *c = new Channel(name, type, size);
    if (c == nullptr)
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' memory error for channel '{}'", this->full_name(), name);
        return nullptr;
    }
    if (this->add_child(c, true) == false)
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' cannot add channel '{}'", this->full_name(), name);
        delete c;
        return nullptr;
    }
    return c;
}

Channel *AChannelContainer::add_channel(const std::string & name, std::string_view type, size_t size)
{
    return this->add_channel(name, sihd::util::type::from_str(type), size);
}

bool AChannelContainer::on_check_link(const std::string & name, Named *child)
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
    if (conf.match && conf.type != chan->array()->data_type())
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' channel link size not same type '{}': '{}' != '{}'",
                       this->full_name(),
                       name,
                       sihd::util::type::str(conf.type),
                       chan->array()->data_type_str());
        ret = false;
    }
    if (conf.match && conf.size != chan->array()->size())
    {
        SIHD_LOG_ERROR("ChannelContainer: '{}' channel link size not equal '{}': '{}' != '{}'",
                       this->full_name(),
                       name,
                       conf.size,
                       chan->array()->size());
        ret = false;
    }
    return ret;
}

bool AChannelContainer::observe_channel(const std::string & channel_name)
{
    Channel *c = this->get_channel(channel_name);
    if (c != nullptr)
        return this->observe_channel(c);
    SIHD_LOG_ERROR("ChannelContainer: '{}' cannot find channel '{}' to observe", this->full_name(), channel_name);
    return false;
}

bool AChannelContainer::observe_channel(Channel *c)
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

void AChannelContainer::remove_channels_observation()
{
    for (Channel *c : _observed_channels)
    {
        c->remove_observer(this);
    }
    _observed_channels.clear();
}

} // namespace sihd::core