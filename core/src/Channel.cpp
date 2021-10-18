#include <sihd/core/Channel.hpp>

namespace sihd::core
{

using namespace sihd::util;

NEW_LOGGER("sihd::core");

sihd::util::IClock *Channel::_default_channel_clock_ptr = &sihd::util::Clock::default_clock;

Channel::Channel(const std::string & name, const std::string & type, size_t size, Node *parent):
    Named(name, parent)
{
    this->_init(Datatype::string_to_datatype(type), size);
}

Channel::Channel(const std::string & name, const std::string & type, Node *parent):
    Named(name, parent)
{
    this->_init(Datatype::string_to_datatype(type), 1);
}

Channel::Channel(const std::string & name, Type type, size_t size, Node *parent):
    Named(name, parent)
{
    this->_init(type, size);
}

Channel::Channel(const std::string & name, Type type, Node *parent):
    Named(name, parent)
{
    this->_init(type, 1);
}

Channel::~Channel()
{
    if (_array_ptr != nullptr)
        delete _array_ptr;
}

Channel     *Channel::build(const std::string & configuration)
{
    auto map = Str::parse_configuration(configuration);
    if (map.find("name") == map.end())
    {
        LOG(error, "Channel: cannot build from configuration '" << configuration << "' no name");
        return nullptr;
    }
    if (map.find("type") == map.end())
    {
        LOG(error, "Channel: cannot build from configuration '" << configuration << "' no type");
        return nullptr;
    }
    if (map.find("size") == map.end())
    {
        LOG(error, "Channel: cannot build from configuration '" << configuration << "' no size");
        return nullptr;
    }
    unsigned long val;
    if (Str::to_ulong(map["size"], &val) == false)
    {
        LOG(error, "Channel: cannot build from configuration '" << configuration
                    << "' size is either overflow or invalid");
        return nullptr;
    }
    return new Channel(map["name"], map["type"], val);
}

void    Channel::_init(Type type, size_t size)
{
    std::lock_guard lock(_arr_mutex);
    _array_ptr = ArrayUtil::create_from_type(type, size);
    if (_array_ptr == nullptr)
    {
        throw std::invalid_argument(Str::format("Channel: no such type %s for channel %s",
                                                    Datatype::datatype_to_string(type).c_str(),
                                                    this->get_name().c_str()));
    }
    _array_ptr->resize(size);
    _notifying = false;
    _write_change_only = true;
    _timestamp = 0;
    _clock_ptr = Channel::get_default_clock();
}

std::time_t     Channel::timestamp()
{
    std::lock_guard lock(_arr_mutex);
    return _timestamp;
}

bool    Channel::copy_to(IArray & arr)
{
    std::lock_guard lock(_arr_mutex);
    return arr.copy_from(*_array_ptr);
}

bool    Channel::write(const IArray & arr)
{
    if (_notifying)
    {
        LOG(warning, "Channel: cannot write while notifying");
        return false;
    }
    bool ret = false;
    {
        std::lock_guard lock(_arr_mutex);
        if (_write_change_only && _array_ptr->is_equal(arr) == true)
            return true;
        ret = _array_ptr->copy_from(arr);
        _timestamp = _clock_ptr->now();
    }
    if (ret)
        this->notify();
    else
    {
        if (_array_ptr->is_same_type(arr) == false)
        {
            LOG(error, "Channel: cannot write an array from different type: "
                << arr.data_type_to_string() << " != " << _array_ptr->data_type_to_string());
        }
        else if (_array_ptr->byte_capacity() <= arr.byte_size())
        {
            LOG(error, "Channel: cannot write an array with too many bytes: "
                    << arr.byte_size() << " > " << _array_ptr->byte_capacity());
        }
    }
    return ret;
}

void    Channel::notify()
{
    std::lock_guard lock(_notify_mutex);
    _notifying = true;
    this->notify_observers(this);
    _notifying = false;
}

}