#include <sihd/core/DevRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Str.hpp>

#define CHANNEL_RECORDS "records"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevRecorder)

LOGGER;

DevRecorder::DevRecorder(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false), _channel_records_ptr(nullptr)
{
    _records_array.resize(1);
    this->add_conf("handler", &DevRecorder::set_handler);
    this->add_conf("record", &DevRecorder::add_record_channel);
    this->add_conf("unrecord", &DevRecorder::remove_recorded_channel);
}

DevRecorder::~DevRecorder()
{
}

bool    DevRecorder::set_handler(const std::string & path)
{
    _handler_path = path;
    return true;
}

bool    DevRecorder::add_record_channel(const std::string & conf)
{
    std::vector<std::string> split = sihd::util::Str::split(conf, "=");
    if (split.size() != 2)
    {
        LOG(error, "DevRecorder: record channel configuration got '"
                    << conf << "' - expected: ALIAS=CHANNEL_PATH");
        return false;
    }
    _map_channels_alias[split[0]] = split[1];
    return true;
}

bool    DevRecorder::remove_recorded_channel(const std::string & alias)
{
    if (_map_channels_alias.find(alias) != _map_channels_alias.end())
        _map_channels_alias.erase(alias);
    return true;
}

bool    DevRecorder::is_running() const
{
    return _running;
}

void    DevRecorder::handle([[maybe_unused]] sihd::core::Channel *channel)
{
    std::string & alias = _map_channels[channel];
    _handler_ptr->handle(alias, channel);
    _records_array[0] += 1;
    _channel_records_ptr->write(_records_array);
}

bool    DevRecorder::on_init()
{
    _handler_ptr = this->find<sihd::util::IHandler<const std::string &, const Channel *>>(_handler_path);
    if (_handler_ptr == nullptr)
    {
        LOG(error, "DevRecorder: handler not found: " << _handler_path);
        return false;
    }
    this->add_unlinked_channel(CHANNEL_RECORDS, sihd::util::DUINT, 1);
    return true;
}

bool    DevRecorder::on_start()
{
    if (this->get_channel(CHANNEL_RECORDS, &_channel_records_ptr) == false)
        return false;
    // find channels and observe them
    Channel *channel_ptr;
    for (const auto & pair: _map_channels_alias)
    {
        channel_ptr = this->find<Channel>(pair.second);
        if (channel_ptr == nullptr)
        {
            LOG(error, "DevRecorder: channel to record '"
                        << pair.first << "' not found: " << pair.second);
            return false;
        }
        _map_channels[channel_ptr] = pair.first;
        this->observe_channel(channel_ptr);
    }
    _running = true;
    return true;
}

bool    DevRecorder::on_stop()
{
    _running = false;
    _channel_records_ptr = nullptr;
    _map_channels.clear();
    return true;
}

bool    DevRecorder::on_reset()
{
    _records_array[0] = 0;
    return true;
}

}