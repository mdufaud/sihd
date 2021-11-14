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

bool    MemRecorder::_check_json_string(const nlohmann::json & json, const std::string & key)
{
    if (json.contains(key))
    {
        if (json[key].is_string())
            return true;
        LOG(error, "MemRecorder: json key '" << key << "' is not a string");
    }
    LOG(error, "MemRecorder: json key '" << key << "' does not exists");
    return false;
}

bool    MemRecorder::_check_json_integer(const nlohmann::json & json, const std::string & key)
{
    if (json.contains(key))
    {
        if (json[key].is_number_unsigned())
            return true;
        LOG(error, "MemRecorder: json key '" << key << "' is not an unsigned number");
    }
    LOG(error, "MemRecorder: json key '" << key << "' does not exists");
    return false;
}

bool    MemRecorder::add_json_records(const nlohmann::json & json)
{
    if (json.is_array() == false)
    {
        LOG(error, "MemRecorder: json provided is not a list");
        return false;
    }
    bool ret = true;
    std::vector<PlayableRecord> records;
    for (auto it = json.begin(); it != json.end(); ++it)
    {
        const nlohmann::json & obj = it.value();
        ret = this->_check_json_string(obj, "name")
            && this->_check_json_string(obj, "data")
            && this->_check_json_string(obj, "type")
            && this->_check_json_integer(obj, "time");
        if (ret)
        {
            PlayableRecord record;
            record.name = obj["name"].get<std::string>();
            record.timestamp = obj["time"].get<time_t>();
            std::string datastring = obj["data"].get<std::string>();
            std::string type = obj["type"].get<std::string>();
            sihd::util::IArray *arr = sihd::util::ArrayUtil::create_from_type(
                sihd::util::Datatype::string_to_datatype(type)
            );
            if (arr != nullptr)
            {
                if (arr->from_string(datastring, ","))
                {
                    record.value = arr;
                    records.push_back(record);
                }
                else
                {
                    LOG_ERROR("MemRecorder: could not get data from datastring of type '%s': '%s'",
                                type.c_str(), datastring.c_str());
                    delete arr;
                }
            }
            else
            {
                LOG(error, "MemRecorder: data type '" << type << "' unknown");
                ret = false;
                break ;
            }
        }
        else
            break ;
    }
    if (ret)
        this->add_records(records);
    else
    {
        for (PlayableRecord & record: records)
        {
            delete record.value;
        }
    }
    return ret;
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