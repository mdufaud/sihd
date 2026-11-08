#include <exception>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/zlib/DevInflate.hpp>

#define OUT_SUFFIX "_out"

namespace sihd::zlib
{

using namespace sihd::util;
using namespace sihd::core;

SIHD_REGISTER_FACTORY(DevInflate)

SIHD_LOGGER;

DevInflate::DevInflate(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _message_mode(false),
    _output_capacity(0),
    _running(false)
{
    constexpr size_t default_output_capacity = 65536;
    this->set_mode("stream");
    this->set_output_capacity(default_output_capacity);
    this->add_conf("mode", &DevInflate::set_mode);
    this->add_conf("output_capacity", &DevInflate::set_output_capacity);
}

DevInflate::~DevInflate() = default;

bool DevInflate::is_running() const
{
    return _running;
}

bool DevInflate::set_mode(std::string_view mode)
{
    if (mode == "stream")
        _message_mode = false;
    else if (mode == "message")
        _message_mode = true;
    else
    {
        SIHD_LOG(error, "DevInflate: mode unknown: '{}' possible values are: stream - message", mode);
        return false;
    }
    return true;
}

bool DevInflate::set_output_capacity(size_t capacity)
{
    if (capacity == 0)
    {
        SIHD_LOG(error, "DevInflate: output capacity cannot be 0");
        return false;
    }
    _output_capacity = capacity;
    return true;
}

void DevInflate::handle(Channel *c)
{
    std::lock_guard lock(_mutex);
    if (_running == false)
        return;
    const auto it = _lanes.find(c);
    if (it == _lanes.end())
        return;

    Lane & lane = it->second;
    const ArrByteView in(*c->array());
    const Result<ArrByte> out = _message_mode ? lane.inflater->inflate_message(in) : lane.inflater->inflate_stream(in);
    if (SIHD_UNEXPECTED_LOG(out))
        return;
    if (lane.out->write(*out) == false)
        SIHD_LOG(error, "DevInflate: {} could not write {}", this->name(), lane.out->name());
}

Result<std::unique_ptr<Inflater>> DevInflate::_new_inflater() const
{
    try
    {
        return std::make_unique<Inflater>(_output_capacity);
    }
    catch (const std::exception & error)
    {
        return std::unexpected(Error(ErrorCode::unknown, "{}", error.what()));
    }
}

Result<void> DevInflate::on_links_resolved()
{
    auto inputs = this->linked_channels();
    SIHD_UNEXPECTED_RETURN(inputs);
    for (const auto & [name, in] : *inputs)
    {
        const std::string out_name = name + OUT_SUFFIX;
        if (this->get_channel(out_name))
            continue;
        Channel *out = this->add_channel_resizable(out_name, TYPE_BYTE, 0, _output_capacity);
        if (out == nullptr)
            return std::unexpected(
                Error(ErrorCode::invalid_argument, "'{}' could not create '{}'", this->full_name(), out_name));
        // every write of the source is decompressed, even an equal one
        out->set_write_on_change(false);
    }
    return {};
}

bool DevInflate::on_start()
{
    auto inputs = this->linked_channels();
    if (SIHD_UNEXPECTED_LOG(inputs))
        return false;

    std::lock_guard lock(_mutex);
    _lanes.clear();
    for (const auto & [name, in] : *inputs)
    {
        auto out = this->get_channel(name + OUT_SUFFIX);
        if (SIHD_UNEXPECTED_LOG(out))
            return false;
        auto inflater = this->_new_inflater();
        if (SIHD_UNEXPECTED_LOG(inflater))
            return false;
        _lanes[in] = Lane {*out, std::move(*inflater)};
    }
    for (const auto & [in, lane] : _lanes)
    {
        if (this->observe_channel(in) == false)
        {
            SIHD_LOG(error, "DevInflate: {} cannot observe {}", this->name(), in->name());
            return false;
        }
    }
    _running = true;
    return true;
}

bool DevInflate::on_stop()
{
    this->remove_channels_observation();
    std::lock_guard lock(_mutex);
    _running = false;
    return true;
}

bool DevInflate::on_reset()
{
    std::lock_guard lock(_mutex);
    _lanes.clear();
    return true;
}

} // namespace sihd::zlib
