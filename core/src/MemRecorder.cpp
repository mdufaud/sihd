#include <nlohmann/json.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#include <sihd/core/MemRecorder.hpp>

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(MemRecorder)

SIHD_LOGGER;

MemRecorder::MemRecorder(const std::string & name, sihd::util::Node *parent): ACoreObject(name, parent)
{
    _running = false;
}

MemRecorder::~MemRecorder()
{
    this->clear();
}

void MemRecorder::add_records(const std::vector<PlayableRecord> & records)
{
    for (const PlayableRecord & record : records)
        this->add_record(record);
}

void MemRecorder::add_records(const std::list<PlayableRecord> & records)
{
    for (const PlayableRecord & record : records)
        this->add_record(record);
}

void MemRecorder::add_record(const std::string & name, sihd::util::Timestamp timestamp, const sihd::util::IArray *array)
{
    sihd::util::IArray *arr = array->clone_array();
    std::lock_guard l(_mutex);
    _map_sorted_records.insert(std::pair<time_t, PlayableRecord>(timestamp.nanoseconds(), {name, timestamp, arr}));
}

void MemRecorder::add_record(const PlayableRecord & record)
{
    this->add_record(record.name, record.timestamp, record.value);
}

bool MemRecorder::empty() const
{
    std::lock_guard l(_mutex);
    return _map_sorted_records.empty();
}

bool MemRecorder::providing() const
{
    return _running;
}

bool MemRecorder::provide(PlayableRecord *value)
{
    std::lock_guard l(_mutex);
    if (_map_sorted_records.empty())
        return false;
    *value = _map_sorted_records.begin()->second;
    _map_sorted_records.erase(_map_sorted_records.begin());
    return true;
}

void MemRecorder::handle(const std::string & name, const Channel *channel)
{
    this->add_record(name, channel->timestamp(), channel->array());
}

MapListRecordedValues MemRecorder::make_recorded_values()
{
    MapListRecordedValues map_record;
    std::lock_guard l(_mutex);
    for (const auto & [_, playable_record] : _map_sorted_records)
    {
        map_record[playable_record.name].push_back({playable_record.timestamp, playable_record.value});
    }
    return map_record;
}

std::string MemRecorder::hexdump_recorded_values(const MapListRecordedValues & recorded_values)
{
    std::string str;
    for (const auto & [channel_name, recorded_values_lst] : recorded_values)
    {
        str += fmt::format("Channel {} ({}):\n", channel_name, recorded_values_lst.size());
        for (const auto & [time, value] : recorded_values_lst)
        {
            str += fmt::format("  {}: {}\n", time, value->hexdump(' '));
        }
    }
    return str;
}

std::string MemRecorder::hexdump_records(std::string_view separation_cols, char separation_data)
{
    std::string str;
    std::lock_guard l(_mutex);

    for (const auto & [_, playable_record] : _map_sorted_records)
    {
        str += fmt::format("{}{}{}{}{}\n",
                           playable_record.name,
                           separation_cols,
                           playable_record.timestamp,
                           separation_cols,
                           playable_record.value->hexdump(separation_data));
    }
    return str;
}

bool MemRecorder::do_start()
{
    _running = true;
    return true;
}

bool MemRecorder::do_stop()
{
    _running = false;
    return true;
}

bool MemRecorder::do_reset()
{
    this->clear();
    return true;
}

void MemRecorder::clear()
{
    std::lock_guard l(_mutex);
    for (const auto & [_, record] : _map_sorted_records)
    {
        delete record.value;
    }
    _map_sorted_records.clear();
}

} // namespace sihd::core