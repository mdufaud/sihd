#include <nlohmann/json.hpp>

#include <sihd/core/MemRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(MemRecorder)

SIHD_LOGGER;

MemRecorder::MemRecorder(const std::string & name, sihd::util::Node *parent):
    ACoreObject(name, parent)
{
    _running = false;
    _records = true;
    _provides = false;
    this->add_conf("provider", &MemRecorder::set_provider);
    this->add_conf("recorder", &MemRecorder::set_recorder);
}

MemRecorder::~MemRecorder()
{
    this->clear();
}

bool    MemRecorder::set_provider(bool active)
{
    _provides = active;
    return true;
}

bool    MemRecorder::set_recorder(bool active)
{
    _records = active;
    return true;
}

void    MemRecorder::add_records(const std::vector<PlayableRecord> & records)
{
    for (const PlayableRecord & record: records)
        this->add_record(record);
}

void    MemRecorder::add_records(const std::list<PlayableRecord> & records)
{
    for (const PlayableRecord & record: records)
        this->add_record(record);
}

void    MemRecorder::add_record(const std::string & name, time_t timestamp, const sihd::util::IArray *array)
{
    if (!_records && !_provides)
        return ;
    sihd::util::IArray *arr = array->clone_array();
    if (_records)
        _map_record[name].push_back({timestamp, arr});
    if (_provides)
    {
        auto lock = std::lock_guard(_mutex);
        _map_sorted_records.insert(std::pair<time_t, PlayableRecord>(
            timestamp, {name, timestamp, arr}
        ));
    }
}

void    MemRecorder::add_record(const PlayableRecord & record)
{
    this->add_record(record.name, record.timestamp, record.value);
}

bool    MemRecorder::providing() const
{
    return _running;
}

bool    MemRecorder::provide(PlayableRecord *value)
{
    auto lock = std::lock_guard(_mutex);
    if (_map_sorted_records.empty())
        return false;
    *value = _map_sorted_records.begin()->second;
    _map_sorted_records.erase(_map_sorted_records.begin());
    return true;
}

void    MemRecorder::handle(const std::string & name, const Channel *channel)
{
    this->add_record(name, channel->timestamp(), channel->array());
}

std::string     MemRecorder::hexdump_records()
{
    std::string str;
    for (const auto & [channel_name, recorded_values_lst]: _map_record)
    {
        str += fmt::format("Channel {} ({}):\n", channel_name, recorded_values_lst.size());
        for (const auto & [time, value]: recorded_values_lst)
        {
            str += fmt::format("  {}: {}\n", time, value->hexdump(' '));
        }
    }
    return str;
}

std::string     MemRecorder::hexdump_timeline(std::string_view separation_cols, char separation_data)
{
    auto lock = std::lock_guard(_mutex);

    std::string str;
    for (const auto & [_, playable_record]: _map_sorted_records)
    {
        str += fmt::format("{}{}{}{}{}\n", playable_record.name,
                            separation_cols, playable_record.timestamp,
                            separation_cols, playable_record.value->hexdump(separation_data));
    }
    return str;
}

bool    MemRecorder::do_start()
{
    _running = true;
    return true;
}

bool    MemRecorder::do_stop()
{
    _running = false;
    return true;
}

bool    MemRecorder::do_reset()
{
    this->clear();
    return true;
}

void    MemRecorder::clear()
{
    {
        auto lock = std::lock_guard(_mutex);
        _map_sorted_records.clear();
    }
    for (const auto & pair: _map_record)
    {
        for (const RecordedValue & value: pair.second)
        {
            delete value.second;
        }
    }
    _map_record.clear();
}

}