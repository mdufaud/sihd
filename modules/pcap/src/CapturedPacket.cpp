#include <cstring>
#include <stdexcept>

#include <sihd/pcap/CapturedPacket.hpp>

namespace sihd::pcap
{

using namespace sihd::util;

namespace
{

constexpr size_t original_length_offset = sizeof(int64_t);
constexpr size_t datalink_offset = original_length_offset + sizeof(uint32_t);

} // namespace

CapturedPacket::CapturedPacket(): _timestamp(0), _original_length(0), _datalink(0) {}

CapturedPacket::CapturedPacket(Timestamp timestamp, uint32_t original_length, int datalink, ArrUByteView data):
    _timestamp(timestamp),
    _original_length(original_length),
    _datalink(datalink),
    _data(data)
{
}

CapturedPacket::CapturedPacket(ArrUByteView packet): CapturedPacket()
{
    auto decoded = this->decode(packet);
    if (!decoded)
        throw std::runtime_error(decoded.error().message);
}

Result<void> CapturedPacket::decode(ArrUByteView packet)
{
    if (packet.byte_size() < header_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "captured packet shorter than its header"));
    int64_t nanoseconds = 0;
    int32_t datalink = 0;
    std::memcpy(&nanoseconds, packet.data(), sizeof(nanoseconds));
    std::memcpy(&_original_length, packet.data() + original_length_offset, sizeof(_original_length));
    std::memcpy(&datalink, packet.data() + datalink_offset, sizeof(datalink));
    _timestamp = Timestamp(nanoseconds);
    _datalink = datalink;
    _data = ArrUByteView(packet.data() + header_size, packet.byte_size() - header_size);
    return {};
}

ArrUByte CapturedPacket::encode() const
{
    const int64_t nanoseconds = _timestamp.nanoseconds();
    const int32_t datalink = _datalink;
    ArrUByte packet;
    packet.resize(header_size + _data.byte_size());
    std::memcpy(packet.buf(), &nanoseconds, sizeof(nanoseconds));
    std::memcpy(packet.buf() + original_length_offset, &_original_length, sizeof(_original_length));
    std::memcpy(packet.buf() + datalink_offset, &datalink, sizeof(datalink));
    if (_data.byte_size() > 0)
        std::memcpy(packet.buf() + header_size, _data.data(), _data.byte_size());
    return packet;
}

Timestamp CapturedPacket::timestamp() const
{
    return _timestamp;
}

uint32_t CapturedPacket::original_length() const
{
    return _original_length;
}

int CapturedPacket::datalink() const
{
    return _datalink;
}

ArrUByteView CapturedPacket::data() const
{
    return _data;
}

} // namespace sihd::pcap
