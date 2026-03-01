#include <nlohmann/json.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

#include <sihd/core/MemRecorder.hpp>

namespace sihd::core
{

SIHD_REGISTER_FACTORY(MemRecorder)

SIHD_LOGGER;

MemRecorder::MemRecorder(const std::string & name, sihd::util::Node *parent): ACoreObject(name, parent)
{
    _running = false;
    _providing = false;
    _stop_providing_when_empty = false;
    this->add_conf("stop_providing_when_empty", &MemRecorder::set_stop_providing_when_empty);
}

MemRecorder::~MemRecorder()
{
    this->clear();
}

bool MemRecorder::set_stop_providing_when_empty(bool active)
{
    _stop_providing_when_empty = active;
    return true;
}

void MemRecorder::add_record(const std::string & name, sihd::util::Timestamp timestamp, const sihd::util::IArray *array)
{
    sihd::util::IArrayShared arr(array->clone_array());
    std::lock_guard l(_mutex);
    _map_sorted_records.insert(std::pair<time_t, PlayableRecord>(timestamp.nanoseconds(), {name, timestamp, arr}));
}

void MemRecorder::add_record(const PlayableRecord & record)
{
    this->add_record(record.name, record.timestamp, record.value.get());
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

bool MemRecorder::empty() const
{
    std::lock_guard l(_mutex);
    return _map_sorted_records.empty();
}

bool MemRecorder::providing() const
{
    return _providing;
}

bool MemRecorder::provide(PlayableRecord *record)
{
    std::lock_guard l(_mutex);
    if (_map_sorted_records.empty())
        return false;
    *record = _map_sorted_records.begin()->second;
    _map_sorted_records.erase(_map_sorted_records.begin());
    if (_stop_providing_when_empty && _map_sorted_records.empty())
        _providing = false;
    return true;
}

void MemRecorder::handle(const std::string & name, const Channel *channel)
{
    this->add_record(name, channel->timestamp(), channel->array());
}

MapListRecordedValues MemRecorder::make_recorded_values() const
{
    std::lock_guard l(_mutex);
    MapListRecordedValues map_record;
    for (const auto & [_, playable_record] : _map_sorted_records)
    {
        map_record[playable_record.name].emplace_back(playable_record.timestamp, playable_record.value);
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
        str += fmt::format("{0}{1}{2}{1}{3}\n",
                           playable_record.name,
                           separation_cols,
                           playable_record.timestamp,
                           playable_record.value->hexdump(separation_data));
    }
    return str;
}

bool MemRecorder::do_start()
{
    _running = true;
    _providing = true;
    return true;
}

bool MemRecorder::do_stop()
{
    _providing = false;
    _running = false;
    return true;
}

bool MemRecorder::do_reset()
{
    this->clear();
    return true;
}

bool MemRecorder::is_running() const
{
    return _running;
}

void MemRecorder::clear()
{
    std::lock_guard l(_mutex);
    _map_sorted_records.clear();
}

} // namespace sihd::core