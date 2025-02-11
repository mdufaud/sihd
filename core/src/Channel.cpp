#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StrConfiguration.hpp>
#include <sihd/util/array_utils.hpp>

#include <sihd/core/Channel.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::core");

sihd::util::IClock *Channel::_default_channel_clock_ptr = &sihd::util::Clock::default_clock;

Channel::Channel(const std::string & name, Type type, size_t size, Node *parent): Named(name, parent)
{
    _array_ptr = array_utils::create_from_type(type, size);
    if (_array_ptr == nullptr)
    {
        throw std::invalid_argument(
            fmt::format("Channel: no such type {} for channel {}", type::str(type), this->name()));
    }
    _array_ptr->resize(size);
    _notifying = false;
    _write_change_only = true;
    _timestamp = 0;
    _clock_ptr = Channel::default_clock();
}

Channel::Channel(const std::string & name, Type type, Node *parent): Channel(name, type, 1, parent) {}

Channel::Channel(const std::string & name, std::string_view type, size_t size, Node *parent):
    Channel(name, type::from_str(type), size, parent)
{
}

Channel::Channel(const std::string & name, std::string_view type, Node *parent):
    Channel(name, type::from_str(type), 1, parent)
{
}

Channel::~Channel()
{
    if (_array_ptr != nullptr)
        delete _array_ptr;
}

Channel *Channel::build(std::string_view configuration)
{
    util::StrConfiguration conf(configuration);

    auto [name, type, size] = conf.find_all("name", "type", "size");

    if (name.has_value() == false)
        SIHD_LOG(error, "Channel: cannot build from configuration '{}' no name", configuration);
    if (type.has_value() == false)
        SIHD_LOG(error, "Channel: cannot build from configuration '{}' no type", configuration);
    if (size.has_value() == false)
        SIHD_LOG(error, "Channel: cannot build from configuration '{}' no size", configuration);

    const bool good = name.has_value() && type.has_value() && size.has_value();
    if (!good)
        return nullptr;

    unsigned long val;
    if (str::to_ulong(*size, &val) == false)
    {
        SIHD_LOG(error,
                 "Channel: cannot build from configuration '{}' size is either overflow or invalid",
                 configuration);
        return nullptr;
    }
    return new Channel(*name, *type, val);
}

Timestamp Channel::timestamp() const
{
    std::lock_guard lock(_arr_mutex);
    return _timestamp;
}

void Channel::do_timestamp()
{
    _timestamp = _clock_ptr->now();
}

bool Channel::copy_to(IArray & arr, size_t byte_offset) const
{
    std::lock_guard lock(_arr_mutex);
    return arr.copy_from_bytes(*_array_ptr, byte_offset);
}

bool Channel::write(const Channel & other)
{
    const IArray *other_array = other.array();
    return other_array != nullptr && this->write(*other_array);
}

bool Channel::write(const sihd::util::ArrByteView & arr_view, size_t byte_offset)
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
            SIHD_LOG_ERROR("Channel: cannot write {} bytes at {} offset into {} bytes",
                           arr_view.byte_size(),
                           byte_offset,
                           _array_ptr->byte_size());
            return false;
        }
        if (_write_change_only && _array_ptr->is_bytes_equal(arr_view.buf(), arr_view.byte_size(), byte_offset))
            return true;
        if ((ret = _array_ptr->copy_from_bytes(arr_view, byte_offset)))
            this->do_timestamp();
    }
    if (ret)
        this->notify();
    return ret;
}

void Channel::notify()
{
    std::lock_guard lock(_notify_mutex);
    _notifying = true;
    this->notify_observers(this);
    _notifying = false;
}

std::string Channel::description() const
{
    if (_array_ptr == nullptr)
        return "empty";
    return fmt::format("{}[{}]", _array_ptr->data_type_str(), _array_ptr->size());
};

} // namespace sihd::core
