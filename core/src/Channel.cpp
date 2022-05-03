#include <sihd/core/Channel.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::core");

sihd::util::IClock *Channel::_default_channel_clock_ptr = &sihd::util::Clock::default_clock;

Channel::Channel(const std::string & name, Type type, size_t size, Node *parent):
    Named(name, parent)
{
    _array_ptr = ArrayUtil::create_from_type(type, size);
    if (_array_ptr == nullptr)
    {
        throw std::invalid_argument(Str::format("Channel: no such type %s for channel %s",
                                                    Types::type_to_string(type),
                                                    this->get_name().c_str()));
    }
    _array_ptr->resize(size);
    _notifying = false;
    _write_change_only = true;
    _timestamp = 0;
    _clock_ptr = Channel::get_default_clock();
}

Channel::Channel(const std::string & name, Type type, Node *parent):
    Channel(name, type, 1, parent)
{
}

Channel::Channel(const std::string & name, std::string_view type, size_t size, Node *parent):
    Channel(name, Types::string_to_type(type), size, parent)
{
}

Channel::Channel(const std::string & name, std::string_view type, Node *parent):
    Channel(name, Types::string_to_type(type), 1, parent)
{
}

Channel::~Channel()
{
    if (_array_ptr != nullptr)
        delete _array_ptr;
}

Channel     *Channel::build(std::string_view configuration)
{
    auto map = Str::parse_configuration(configuration);
    if (map.find("name") == map.end())
    {
        SIHD_LOG(error, "Channel: cannot build from configuration '" << configuration << "' no name");
        return nullptr;
    }
    if (map.find("type") == map.end())
    {
        SIHD_LOG(error, "Channel: cannot build from configuration '" << configuration << "' no type");
        return nullptr;
    }
    if (map.find("size") == map.end())
    {
        SIHD_LOG(error, "Channel: cannot build from configuration '" << configuration << "' no size");
        return nullptr;
    }
    unsigned long val;
    if (Str::to_ulong(map["size"], &val) == false)
    {
        SIHD_LOG(error, "Channel: cannot build from configuration '" << configuration
                    << "' size is either overflow or invalid");
        return nullptr;
    }
    return new Channel(map["name"], map["type"], val);
}

std::time_t     Channel::timestamp()
{
    std::lock_guard lock(_arr_mutex);
    return _timestamp;
}

bool    Channel::copy_to(IArray & arr, size_t byte_offset)
{
    std::lock_guard lock(_arr_mutex);
    return arr.copy_from_bytes(*_array_ptr, byte_offset);
}

void    Channel::do_timestamp()
{
    _timestamp = _clock_ptr->now();
}

bool    Channel::write(const Channel & other)
{
    const IArray *other_array = other.array();
    return other_array != nullptr && this->write(*other_array);
}

bool    Channel::write(const sihd::util::ArrViewByte & arr_view, size_t byte_offset)
{
    if (_notifying)
    {
        SIHD_LOG(warning, "Channel: cannot write while notifying");
        return false;
    }
    bool ret = false;
    {
        std::lock_guard lock(_arr_mutex);
        if (arr_view.byte_size() + byte_offset > _array_ptr->byte_size())
        {
            SIHD_LOG_ERROR("Channel: cannot write %lu bytes at %lu offset into %lu bytes",
                            arr_view.byte_size(), byte_offset, _array_ptr->byte_size());
            return false;
        }
        if (_write_change_only
                && _array_ptr->is_bytes_equal(arr_view.buf(), arr_view.byte_size(), byte_offset))
            return true;
        if ((ret = _array_ptr->copy_from_bytes(arr_view, byte_offset)) == true)
            this->do_timestamp();
    }
    if (ret)
        this->notify();
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