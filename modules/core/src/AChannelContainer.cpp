#include <sihd/core/AChannelContainer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::core
{

SIHD_LOGGER;

using enum sihd::util::ErrorCode;
using namespace sihd::util;

AChannelContainer::AChannelContainer(const std::string & name, Node *parent): Node(name, parent) {}

AChannelContainer::~AChannelContainer() = default;

Result<Channel *> AChannelContainer::find_channel(const std::string & path)
{
    Channel *channel = this->find<Channel>(path);
    if (channel == nullptr)
        return std::unexpected(Error(not_found, "no such channel '{}' in '{}'", path, this->full_name()));
    return channel;
}

Result<Channel *> AChannelContainer::get_channel(const std::string & name)
{
    Named *child = this->get_child(name);
    Channel *channel = child != nullptr ? dynamic_cast<Channel *>(child) : nullptr;
    if (channel == nullptr)
        return std::unexpected(Error(not_found, "no such channel '{}' in '{}'", name, this->full_name()));
    return channel;
}

Result<std::map<std::string, Channel *>> AChannelContainer::linked_channels()
{
    std::map<std::string, Channel *> channels;
    for (const std::string & name : this->children_keys())
    {
        if (this->is_link(name) == false || _channels_link.contains(name))
            continue;
        auto channel = this->get_channel(name);
        SIHD_UNEXPECTED_RETURN(channel);
        channels[name] = *channel;
    }
    return channels;
}

Channel *AChannelContainer::add_unlinked_channel(const std::string & name, Type type, size_t size, bool check_match)
{
    if (this->is_link(name))
    {
        constexpr bool resizable = false;
        _channels_link[name] = {type, size, size, check_match, resizable};
        return nullptr;
    }
    return this->add_channel(name, type, size);
}

Channel *AChannelContainer::add_unlinked_channel(const std::string & name,
                                                 std::string_view type_str,
                                                 size_t size,
                                                 bool check_match)
{
    return this->add_unlinked_channel(name, type::from_str(type_str), size, check_match);
}

Channel *AChannelContainer::add_unlinked_channel_resizable(const std::string & name,
                                                           Type type,
                                                           size_t size,
                                                           size_t capacity,
                                                           bool check_match)
{
    if (this->is_link(name))
    {
        constexpr bool resizable = true;
        _channels_link[name] = {type, size, capacity, check_match, resizable};
        return nullptr;
    }
    return this->add_channel_resizable(name, type, size, capacity);
}

Channel *AChannelContainer::add_unlinked_channel_resizable(const std::string & name,
                                                           std::string_view type_str,
                                                           size_t size,
                                                           size_t capacity,
                                                           bool check_match)
{
    return this->add_unlinked_channel_resizable(name, type::from_str(type_str), size, capacity, check_match);
}

Channel *AChannelContainer::add_channel(const std::string & name, Type type, size_t size)
{
    Channel *channel = new Channel(name, type, size);
    auto added = this->add_child(channel, true);
    if (added.has_value() == false)
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' cannot add channel '{}': {}",
                 this->full_name(),
                 name,
                 added.error().message);
        delete channel;
        return nullptr;
    }
    return channel;
}

Channel *AChannelContainer::add_channel(const std::string & name, std::string_view type_str, size_t size)
{
    return this->add_channel(name, type::from_str(type_str), size);
}

Channel *AChannelContainer::add_channel_resizable(const std::string & name, Type type, size_t size, size_t capacity)
{
    Channel *channel = this->add_channel(name, type, size);
    if (channel == nullptr)
        return nullptr;
    channel->reserve(capacity);
    channel->set_resizable(true);
    return channel;
}

Channel *AChannelContainer::add_channel_resizable(const std::string & name,
                                                  std::string_view type_str,
                                                  size_t size,
                                                  size_t capacity)
{
    return this->add_channel_resizable(name, type::from_str(type_str), size, capacity);
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
    if (conf.resizable != chan->resizable())
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' channel link resizable mismatch '{}': expected {} got {}",
                 this->full_name(),
                 name,
                 conf.resizable,
                 chan->resizable());
        ret = false;
    }

    if (conf.match && conf.type != chan->array()->data_type())
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' channel link size not same type '{}': '{}' != '{}'",
                 this->full_name(),
                 name,
                 type::str(conf.type),
                 chan->array()->data_type_str());
        ret = false;
    }

    if (conf.match && !conf.resizable && conf.size != chan->array()->size())
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' channel link size not equal '{}': '{}' != '{}'",
                 this->full_name(),
                 name,
                 conf.size,
                 chan->array()->size());
        ret = false;
    }

    // a resizable channel size follows its writes, only its capacity is a property of the port
    if (conf.match && conf.resizable && conf.capacity > chan->capacity())
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' channel link capacity too small '{}': '{}' < '{}'",
                 this->full_name(),
                 name,
                 chan->capacity(),
                 conf.capacity);
        ret = false;
    }
    return ret;
}

Result<void> AChannelContainer::observe_channel(const std::string & channel_name)
{
    auto channel = this->get_channel(channel_name);
    SIHD_UNEXPECTED_RETURN(channel);
    this->observe_channel(*channel);
    return {};
}

bool AChannelContainer::observe_channel(Channel *channel)
{
    if (channel != nullptr)
    {
        // false if already added
        if (channel->add_observer(this))
            _observed_channels.push_back(channel);
        return true;
    }
    return false;
}

void AChannelContainer::remove_channels_observation()
{
    for (Channel *channel : _observed_channels)
    {
        channel->remove_observer(this);
    }
    _observed_channels.clear();
}

} // namespace sihd::core