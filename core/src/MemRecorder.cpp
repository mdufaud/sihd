#include <sihd/core/MemRecorder.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(MemRecorder)

LOGGER;

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
    sihd::util::IArray *arr = sihd::util::ArrayUtil::clone_array(*array);
    if (_records)
        _map_record[name].push_back({timestamp, arr});
    if (_provides)
    {
        {
            this->provider_lock_guard();
            _map_sorted_records.insert(std::pair<time_t, PlayableRecord>(
                timestamp, {name, timestamp, arr}
            ));
        }
        this->_provider_notify();
    }
}

void    MemRecorder::add_record(const PlayableRecord & record)
{
    this->add_record(record.name, record.timestamp, record.value);
}

bool    MemRecorder::provider_empty() const
{
    return _map_sorted_records.empty();
}

bool    MemRecorder::providing() const
{
    return _running;
}

bool    MemRecorder::provide(PlayableRecord *value)
{
    if (this->provider_empty())
        return false;
    *value = _map_sorted_records.begin()->second;
    _map_sorted_records.erase(_map_sorted_records.begin());
    return true;
}

void    MemRecorder::handle(const std::string & name, const Channel *channel)
{
    this->add_record(name, channel->ctimestamp(), channel->carray());
}

std::string     MemRecorder::hexdump_records()
{
    std::stringstream ss;
    for (const auto & map_pair: _map_record)
    {
        ss << "Channel " << map_pair.first << " (" << map_pair.second.size() << "):" << std::endl;
        for (const RecordedValue & value: map_pair.second)
        {
            ss << value.first << ": " << value.second->hexdump(' ') << std::endl;
        }
    }
    return ss.str();
}

std::string     MemRecorder::hexdump_timeline(const std::string & separation_cols, char separation_data)
{
    std::stringstream ss;
    for (const auto & map_pair: _map_sorted_records)
    {
        ss << map_pair.second.name << separation_cols
            << map_pair.second.timestamp << separation_cols
            << map_pair.second.value->hexdump(separation_data)
            << std::endl;
    }
    return ss.str();
}

bool    MemRecorder::do_start()
{
    _running = true;
    this->_provider_notify_all();
    return true;
}

bool    MemRecorder::do_stop()
{
    _running = false;
    this->_provider_notify_all();
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
        this->provider_lock_guard();
        _map_sorted_records.clear();
    }
    this->_provider_notify_all();
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