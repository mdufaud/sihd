#include <sihd/core/Channel.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StrConfiguration.hpp>
#include <sihd/util/array_utils.hpp>

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
    _resizable = false;
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

    const auto val = str::convert_from_string<unsigned long>(*size);
    if (val.has_value() == false)
    {
        SIHD_LOG(error,
                 "Channel: cannot build from configuration '{}' size is either overflow or invalid",
                 configuration);
        return nullptr;
    }

    Channel *channel = new Channel(*name, *type, *val);

    auto capacity = conf.find("capacity");
    if (capacity.has_value())
    {
        const auto cap = str::convert_from_string<unsigned long>(*capacity);
        if (cap.has_value() && *cap > *val)
        {
            channel->reserve(*cap);
            channel->set_resizable(true);
        }
    }

    return channel;
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

bool Channel::copy_to(IArray & arr, Timestamp *timestamp) const
{
    return this->copy_to(arr, {}, timestamp);
}

bool Channel::copy_to_bytes(IArray & arr, Slice byte_slice, Timestamp *timestamp) const
{
    std::lock_guard lock(_arr_mutex);
    if (timestamp != nullptr)
        *timestamp = _timestamp;
    auto range = byte_slice.resolve(_array_ptr->byte_size());
    if (range.empty())
        return false;
    return arr.copy_from_bytes(_array_ptr->buf() + range.from, range.size());
}

bool Channel::copy_to(IArray & arr, Slice slice, Timestamp *timestamp) const
{
    std::lock_guard lock(_arr_mutex);
    if (timestamp != nullptr)
        *timestamp = _timestamp;
    auto range = slice.resolve(_array_ptr->size());
    if (range.empty())
        return false;
    return arr.copy_from_bytes(_array_ptr->buf_at(range.from), range.size() * _array_ptr->data_size());
}

bool Channel::write(const Channel & other)
{
    std::unique_ptr<IArray> copy;
    {
        std::lock_guard lock(other._arr_mutex);
        const IArray *other_array = other.array();
        if (other_array == nullptr)
            return false;
        copy.reset(other_array->clone_array());
    }
    return this->write(*copy);
}

bool Channel::write(const sihd::util::ArrByteView & arr_view, size_t byte_offset)
{
    std::unique_lock notify_lock(_notify_mutex, std::try_to_lock);
    if (!notify_lock.owns_lock())
    {
        SIHD_LOG(warning, "Channel: cannot write while notifying");
        return false;
    }

    bool ret = false;
    {
        std::lock_guard lock(_arr_mutex);
        const size_t needed = arr_view.byte_size() + byte_offset;
        if (needed > _array_ptr->byte_size())
        {
            if (!_resizable || needed > _array_ptr->byte_capacity())
            {
                SIHD_LOG_ERROR("Channel: cannot write {} bytes at {} offset into {} bytes",
                               arr_view.byte_size(),
                               byte_offset,
                               _array_ptr->byte_size());
                return false;
            }
            _array_ptr->byte_resize(needed);
        }
        if (_write_change_only && _array_ptr->is_bytes_equal(arr_view, {(ssize_t)byte_offset}))
            return true;
        if ((ret = _array_ptr->copy_from_bytes(arr_view, {(ssize_t)byte_offset})))
            this->do_timestamp();
    }
    if (ret)
    {
        _notifying = true;
        this->notify_observers(this);
        _notifying = false;
    }
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
