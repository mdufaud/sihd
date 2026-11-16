#include <cstring>
#include <limits>
#include <stdexcept>

#include <sihd/net/LoggerRecord.hpp>
#include <sihd/util/endian.hpp>

namespace sihd::net
{

using namespace sihd::util;

namespace
{

// version u8, level u8, timestamp u64, then a u16, u16 and u32 length before each text
constexpr size_t fixed_size = 1 + 1 + 8 + 2 + 2 + 4;

class Reader
{
    public:
        Reader(ArrByteView bytes): _bytes(bytes), _offset(0) {}

        size_t remaining() const { return _bytes.byte_size() - _offset; }

        template <typename T>
        bool read_be(T & out)
        {
            if (sizeof(T) > this->remaining())
                return false;
            out = endian::read_be<T>(_bytes.buf() + _offset, sizeof(T));
            _offset += sizeof(T);
            return true;
        }

        bool read_view(size_t size, std::string_view & out)
        {
            if (size > this->remaining())
                return false;
            out = std::string_view(reinterpret_cast<const char *>(_bytes.buf()) + _offset, size);
            _offset += size;
            return true;
        }

    private:
        ArrByteView _bytes;
        size_t _offset;
};

class Writer
{
    public:
        Writer(uint8_t *data): _data(data) {}

        template <typename T>
        void write_be(T value)
        {
            endian::write_be(value, _data, sizeof(T));
            _data += sizeof(T);
        }

        void write_view(std::string_view view)
        {
            if (view.empty() == false)
                std::memcpy(_data, view.data(), view.size());
            _data += view.size();
        }

    private:
        uint8_t *_data;
};

template <typename Length>
std::string_view clamp_to(std::string_view view)
{
    return view.substr(0, std::numeric_limits<Length>::max());
}

} // namespace

LoggerRecord::LoggerRecord(): _level(LogLevel::none), _timestamp(0) {}

LoggerRecord::LoggerRecord(const LogInfo & info, std::string_view message):
    _level(info.level),
    _timestamp(info.timestamp()),
    _source(clamp_to<uint16_t>(info.source)),
    _thread_name(clamp_to<uint16_t>(info.thread_name)),
    _message(clamp_to<uint32_t>(message))
{
}

LoggerRecord::LoggerRecord(ArrByteView record): LoggerRecord()
{
    auto decoded = this->decode(record);
    if (!decoded)
        throw std::runtime_error(decoded.error().message);
}

Result<void> LoggerRecord::decode(ArrByteView record)
{
    Reader reader(record);
    uint8_t record_version = 0;
    if (reader.read_be(record_version) == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "empty logger record"));
    if (record_version != version)
        return std::unexpected(Error(ErrorCode::not_supported, "logger record version {}", record_version));

    const auto truncated = std::unexpected(Error(ErrorCode::invalid_argument, "logger record truncated"));
    LoggerRecord decoded;
    uint8_t level = 0;
    uint64_t nanoseconds = 0;
    if (reader.read_be(level) == false || reader.read_be(nanoseconds) == false)
        return truncated;

    uint16_t source_size = 0;
    if (reader.read_be(source_size) == false || reader.read_view(source_size, decoded._source) == false)
        return truncated;
    uint16_t thread_name_size = 0;
    if (reader.read_be(thread_name_size) == false || reader.read_view(thread_name_size, decoded._thread_name) == false)
        return truncated;
    uint32_t message_size = 0;
    if (reader.read_be(message_size) == false || reader.read_view(message_size, decoded._message) == false)
        return truncated;

    if (reader.remaining() > 0)
        return std::unexpected(
            Error(ErrorCode::invalid_argument, "logger record followed by {} bytes", reader.remaining()));
    if (level > static_cast<uint8_t>(LogLevel::debug))
        return std::unexpected(Error(ErrorCode::invalid_argument, "logger record level {}", level));
    decoded._level = static_cast<LogLevel>(level);
    decoded._timestamp = Timestamp(static_cast<int64_t>(nanoseconds));
    *this = decoded;
    return {};
}

ArrByte LoggerRecord::encode() const
{
    ArrByte bytes;
    bytes.resize(this->encoded_size());
    this->encode_to(bytes.buf());
    return bytes;
}

size_t LoggerRecord::encoded_size() const
{
    return fixed_size + _source.size() + _thread_name.size() + _message.size();
}

void LoggerRecord::encode_to(uint8_t *data) const
{
    Writer writer(data);
    writer.write_be<uint8_t>(version);
    writer.write_be<uint8_t>(static_cast<uint8_t>(_level));
    writer.write_be<uint64_t>(static_cast<uint64_t>(_timestamp.get()));
    writer.write_be<uint16_t>(static_cast<uint16_t>(_source.size()));
    writer.write_view(_source);
    writer.write_be<uint16_t>(static_cast<uint16_t>(_thread_name.size()));
    writer.write_view(_thread_name);
    writer.write_be<uint32_t>(static_cast<uint32_t>(_message.size()));
    writer.write_view(_message);
}

LogInfo LoggerRecord::log_info() const
{
    LogInfo info(_source, _level);
    info.timespec = _timestamp.ts();
    info.thread_name = _thread_name;
    return info;
}

LogLevel LoggerRecord::level() const
{
    return _level;
}

Timestamp LoggerRecord::timestamp() const
{
    return _timestamp;
}

std::string_view LoggerRecord::source() const
{
    return _source;
}

std::string_view LoggerRecord::thread_name() const
{
    return _thread_name;
}

std::string_view LoggerRecord::message() const
{
    return _message;
}

} // namespace sihd::net
