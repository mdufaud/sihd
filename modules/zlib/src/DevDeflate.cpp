#include <exception>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/zlib/DevDeflate.hpp>

#define OUT_SUFFIX "_out"

namespace sihd::zlib
{

using namespace sihd::util;
using namespace sihd::core;

SIHD_REGISTER_FACTORY(DevDeflate)

SIHD_NEW_LOGGER("sihd::zlib");

DevDeflate::DevDeflate(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _message_mode(false),
    _level(0),
    _format(Deflater::Format::zlib),
    _running(false)
{
    this->set_mode("stream");
    this->set_level(Deflater::default_level);
    this->set_format("zlib");
    this->add_conf("mode", &DevDeflate::set_mode);
    this->add_conf("level", &DevDeflate::set_level);
    this->add_conf("format", &DevDeflate::set_format);
}

DevDeflate::~DevDeflate() = default;

bool DevDeflate::is_running() const
{
    return _running;
}

bool DevDeflate::set_mode(std::string_view mode)
{
    if (mode == "stream")
        _message_mode = false;
    else if (mode == "message")
        _message_mode = true;
    else
    {
        SIHD_LOG(error, "DevDeflate: mode unknown: '{}' possible values are: stream - message", mode);
        return false;
    }
    return true;
}

bool DevDeflate::set_level(int level)
{
    if (level < Deflater::min_level || level > Deflater::max_level)
    {
        SIHD_LOG(error, "DevDeflate: level {} is outside {}..{}", level, Deflater::min_level, Deflater::max_level);
        return false;
    }
    _level = level;
    return true;
}

bool DevDeflate::set_format(std::string_view format)
{
    if (format == "zlib")
        _format = Deflater::Format::zlib;
    else if (format == "gzip")
        _format = Deflater::Format::gzip;
    else
    {
        SIHD_LOG(error, "DevDeflate: format unknown: '{}' possible values are: zlib - gzip", format);
        return false;
    }
    return true;
}

void DevDeflate::handle(Channel *c)
{
    std::lock_guard lock(_mutex);
    if (_running == false)
        return;
    const auto it = _lanes.find(c);
    if (it == _lanes.end())
        return;

    Lane & lane = it->second;
    const ArrByteView in(*c->array());
    const Result<ArrByte> out = _message_mode ? lane.deflater->deflate_message(in) : lane.deflater->deflate_stream(in);
    if (SIHD_UNEXPECTED_LOG(out))
        return;
    if (lane.out->write(*out) == false)
        SIHD_LOG(error, "DevDeflate: {} could not write {}", this->name(), lane.out->name());
}

Result<std::unique_ptr<Deflater>> DevDeflate::_new_deflater() const
{
    try
    {
        return std::make_unique<Deflater>(_format, _level);
    }
    catch (const std::exception & error)
    {
        return std::unexpected(Error(ErrorCode::unknown, "{}", error.what()));
    }
}

Result<void> DevDeflate::on_links_resolved()
{
    auto inputs = this->linked_channels();
    SIHD_UNEXPECTED_RETURN(inputs);
    auto deflater = this->_new_deflater();
    SIHD_UNEXPECTED_RETURN(deflater);

    for (const auto & [name, in] : *inputs)
    {
        const std::string out_name = name + OUT_SUFFIX;
        if (this->get_channel(out_name))
            continue;
        const size_t in_capacity = in->resizable() ? in->byte_capacity() : in->byte_size();
        const size_t out_capacity = (*deflater)->output_bound(in_capacity);
        Channel *out = this->add_channel_resizable(out_name, TYPE_BYTE, 0, out_capacity);
        if (out == nullptr)
            return std::unexpected(
                Error(ErrorCode::invalid_argument, "'{}' could not create '{}'", this->full_name(), out_name));
        // every write of the source is compressed, even an equal one
        out->set_write_on_change(false);
    }
    return {};
}

bool DevDeflate::on_start()
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
        auto deflater = this->_new_deflater();
        if (SIHD_UNEXPECTED_LOG(deflater))
            return false;
        _lanes[in] = Lane {*out, std::move(*deflater)};
    }
    for (const auto & [in, lane] : _lanes)
    {
        if (this->observe_channel(in) == false)
        {
            SIHD_LOG(error, "DevDeflate: {} cannot observe {}", this->name(), in->name());
            return false;
        }
    }
    _running = true;
    return true;
}

bool DevDeflate::on_stop()
{
    this->remove_channels_observation();
    std::lock_guard lock(_mutex);
    _running = false;
    return true;
}

bool DevDeflate::on_reset()
{
    std::lock_guard lock(_mutex);
    _lanes.clear();
    return true;
}

} // namespace sihd::zlib
