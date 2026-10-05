#include <sihd/core/AChannelContainer.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::core
{

SIHD_LOGGER;

using enum sihd::util::ErrorCode;
using namespace sihd::util;

AChannelContainer::AChannelContainer(const std::string & name, Node *parent): Node(name, parent) {}

AChannelContainer::~AChannelContainer() = default;

std::expected<Channel *, Error> AChannelContainer::find_channel(const std::string & path)
{
    Channel *c = this->find<Channel>(path);
    if (c == nullptr)
        return std::unexpected(Error(not_found, "no such channel '{}' in '{}'", path, this->full_name()));
    return c;
}

std::expected<Channel *, Error> AChannelContainer::get_channel(const std::string & name)
{
    Named *child = this->get_child(name);
    Channel *c = child != nullptr ? dynamic_cast<Channel *>(child) : nullptr;
    if (c == nullptr)
        return std::unexpected(Error(not_found, "no such channel '{}' in '{}'", name, this->full_name()));
    return c;
}

Channel *AChannelContainer::add_unlinked_channel(const std::string & name,
                                                 sihd::util::Type type,
                                                 size_t size,
                                                 bool check_match)
{
    if (this->is_link(name))
    {
        constexpr bool resizable = false;
        _channels_link[name] = {type, size, check_match, resizable};
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

Channel *AChannelContainer::add_unlinked_channel_resizable(const std::string & name,
                                                           sihd::util::Type type,
                                                           size_t size,
                                                           size_t capacity,
                                                           bool check_match)
{
    if (this->is_link(name))
    {
        constexpr bool resizable = true;
        _channels_link[name] = {type, size, check_match, resizable};
        return nullptr;
    }
    return this->add_channel_resizable(name, type, size, capacity);
}

Channel *AChannelContainer::add_unlinked_channel_resizable(const std::string & name,
                                                           std::string_view type,
                                                           size_t size,
                                                           size_t capacity,
                                                           bool check_match)
{
    return this->add_unlinked_channel_resizable(name, sihd::util::type::from_str(type), size, capacity, check_match);
}

Channel *AChannelContainer::add_channel(const std::string & name, sihd::util::Type type, size_t size)
{
    Channel *c = new Channel(name, type, size);
    auto added = this->add_child(c, true);
    if (added.has_value() == false)
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' cannot add channel '{}': {}",
                 this->full_name(),
                 name,
                 added.error().message);
        delete c;
        return nullptr;
    }
    return c;
}

Channel *AChannelContainer::add_channel(const std::string & name, std::string_view type, size_t size)
{
    return this->add_channel(name, sihd::util::type::from_str(type), size);
}

Channel *AChannelContainer::add_channel_resizable(const std::string & name,
                                                  sihd::util::Type type,
                                                  size_t size,
                                                  size_t capacity)
{
    Channel *c = this->add_channel(name, type, size);
    if (c == nullptr)
        return nullptr;
    c->reserve(capacity);
    c->set_resizable(true);
    return c;
}

Channel *AChannelContainer::add_channel_resizable(const std::string & name,
                                                  std::string_view type,
                                                  size_t size,
                                                  size_t capacity)
{
    return this->add_channel_resizable(name, sihd::util::type::from_str(type), size, capacity);
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
                 sihd::util::type::str(conf.type),
                 chan->array()->data_type_str());
        ret = false;
    }
    if (conf.match && conf.size != chan->array()->size())
    {
        SIHD_LOG(error,
                 "ChannelContainer: '{}' channel link size not equal '{}': '{}' != '{}'",
                 this->full_name(),
                 name,
                 conf.size,
                 chan->array()->size());
        ret = false;
    }
    return ret;
}

std::expected<void, Error> AChannelContainer::observe_channel(const std::string & channel_name)
{
    auto c = this->get_channel(channel_name);
    SIHD_UNEXPECTED_RETURN(c);
    this->observe_channel(*c);
    return {};
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