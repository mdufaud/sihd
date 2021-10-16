#include <sihd/core/DevRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define CHANNEL_CLEAR "clear"
#define CHANNEL_RECORDS "records"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevRecorder)

LOGGER;

DevRecorder::DevRecorder(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false)
{
    _records_array.resize(1);
    this->add_conf("record", &DevRecorder::add_record_channel);
    this->add_conf("unrecord", &DevRecorder::remove_recorded_channel);
}

DevRecorder::~DevRecorder()
{
    this->clear_recorded_values();
}

std::string     DevRecorder::to_string(const DevRecorder::map_recorded_channels & map)
{
    std::stringstream ss;
    for (const auto & map_pair: map)
    {
        ss << "Channel " << map_pair.first << " (" << map_pair.second.size() << "):" << std::endl;
        for (const recorded_value & value: map_pair.second)
        {
            ss << value.first << ": " << value.second->hexdump(' ') << std::endl;
        }
    }
    return ss.str();
}

bool    DevRecorder::add_record_channel(const std::string & path)
{
    _set_channels_to_record.insert(path);
    return true;
}

bool    DevRecorder::remove_recorded_channel(const std::string & path)
{
    _set_channels_to_record.erase(path);
    return true;
}

bool    DevRecorder::is_running() const
{
    return _running;
}

void    DevRecorder::observable_changed([[maybe_unused]] sihd::core::Channel *channel)
{
    if (channel == _channel_clear_ptr)
    {
        this->clear_recorded_values();
        _channel_records_ptr->write(_records_array);
        return ;
    }
    const sihd::util::IArray *chan_arr = channel->array();
    std::lock_guard lock(_mutex_recorded_values);
    lst_recorded_values & values = _map_channels_values[_map_channels[channel]];
    sihd::util::IArray *typed_array = sihd::util::ArrayUtil::clone_array(*chan_arr);
    values.push_back({channel->timestamp(), typed_array});
    _records_array[0] += 1;
    _channel_records_ptr->write(_records_array);
}

bool    DevRecorder::on_init()
{
    this->add_unlinked_channel(CHANNEL_CLEAR, sihd::util::DBOOL, 1);
    this->add_unlinked_channel(CHANNEL_RECORDS, sihd::util::DUINT, 1);
    return true;
}

bool    DevRecorder::on_start()
{
    if (this->get_channel(CHANNEL_CLEAR, &_channel_clear_ptr) == false)
        return false;
    if (this->get_channel(CHANNEL_RECORDS, &_channel_records_ptr) == false)
        return false;
    this->observe_channel(_channel_clear_ptr);
    // find channels and observe them
    Channel *channel_ptr;
    for (const std::string & channel_path: _set_channels_to_record)
    {
        channel_ptr = this->find<Channel>(channel_path);
        if (channel_ptr == nullptr)
        {
            LOG(error, "DevRecorder: channel to record not found: " << channel_path);
            return false;
        }
        _map_channels[channel_ptr] = channel_path;
        this->observe_channel(channel_ptr);
    }

    _running = true;
    return true;
}

bool    DevRecorder::on_stop()
{
    _running = false;
    _channel_clear_ptr = nullptr;
    _channel_records_ptr = nullptr;
    _map_channels.clear();
    return true;
}

bool    DevRecorder::on_reset()
{
    this->clear_recorded_values();
    _records_array[0] = 0;
    _set_channels_to_record.clear();
    return true;
}

void    DevRecorder::clear_recorded_values()
{
    std::lock_guard lock(_mutex_recorded_values);
    for (auto & map_pair: _map_channels_values)
    {
        for (recorded_value & pair: map_pair.second)
        {
            delete pair.second;
        }
    }
    _map_channels_values.clear();
}

}