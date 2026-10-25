#include <algorithm>
#include <charconv>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/frame.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t control_short_size = 10;
constexpr size_t control_size = 16;
constexpr size_t mac_header_size = 24;
constexpr size_t addr4_size = 6;
constexpr size_t qos_control_size = 2;
constexpr size_t ht_control_size = 4;

constexpr size_t addr1_offset = 4;
constexpr size_t addr2_offset = 10;
constexpr size_t addr3_offset = 16;
constexpr size_t sequence_control_offset = 22;
constexpr size_t addr4_offset = 24;

constexpr uint16_t fc_version_mask = 0x0003;
constexpr unsigned fc_type_shift = 2;
constexpr uint16_t fc_type_mask = 0x0003;
constexpr unsigned fc_subtype_shift = 4;
constexpr uint16_t fc_subtype_mask = 0x000f;
constexpr uint16_t fc_to_ds = 0x0100;
constexpr uint16_t fc_from_ds = 0x0200;
constexpr uint16_t fc_more_fragments = 0x0400;
constexpr uint16_t fc_retry = 0x0800;
constexpr uint16_t fc_power_management = 0x1000;
constexpr uint16_t fc_more_data = 0x2000;
constexpr uint16_t fc_protected = 0x4000;
constexpr uint16_t fc_order = 0x8000;

constexpr uint8_t data_subtype_qos = 0x08;
constexpr uint16_t qos_tid_mask = 0x000f;
constexpr uint16_t qos_amsdu_bit = 0x0080;
constexpr uint16_t fragment_mask = 0x000f;
constexpr unsigned sequence_shift = 4;

// the subtype numbers the standard leaves reserved: 0 and 1 below the first control subtype
constexpr uint8_t management_reserved_subtype_a = 7;
constexpr uint8_t management_reserved_subtype_b = 15;
constexpr uint8_t control_first_subtype = 2;

constexpr size_t mac_digits_size = 2;
constexpr size_t mac_octet_text_size = 3;
constexpr size_t mac_text_size = 17;

constexpr uint8_t group_bit = 0x01;
constexpr uint8_t local_bit = 0x02;

bool is_qos_data(const Frame & frame)
{
    return frame.type == Frame::Type::data && (frame.subtype & data_subtype_qos) != 0;
}

bool is_short_control(const Frame & frame)
{
    const std::optional<Frame::ControlSubtype> subtype = control_subtype(frame);
    return subtype == Frame::ControlSubtype::ack || subtype == Frame::ControlSubtype::cts;
}

// a control wrapper is 16 bytes like an RTS, but holds the carried frame control and HT control, not a transmitter
bool has_transmitter(const Frame & frame)
{
    return is_short_control(frame) == false && control_subtype(frame) != Frame::ControlSubtype::control_wrapper;
}

bool has_ht_control(const Frame & frame)
{
    return frame.order && (frame.type == Frame::Type::management || is_qos_data(frame));
}

// MAC header length, layout in docs/protocols/wifi-frames.md
size_t header_size_of(const Frame & frame)
{
    if (frame.type == Frame::Type::control)
        return is_short_control(frame) ? control_short_size : control_size;

    size_t size = mac_header_size;
    if (frame.type == Frame::Type::data && frame.to_ds && frame.from_ds)
        size += addr4_size;
    if (is_qos_data(frame))
        size += qos_control_size;
    if (has_ht_control(frame))
        size += ht_control_size;
    return size;
}

MacAddress mac_at(const uint8_t *data, size_t offset)
{
    MacAddress mac;
    std::copy_n(data + offset, mac.size(), mac.begin());
    return mac;
}

void read_flags(uint16_t frame_control, Frame & frame)
{
    frame.to_ds = (frame_control & fc_to_ds) != 0;
    frame.from_ds = (frame_control & fc_from_ds) != 0;
    frame.more_fragments = (frame_control & fc_more_fragments) != 0;
    frame.retry = (frame_control & fc_retry) != 0;
    frame.power_management = (frame_control & fc_power_management) != 0;
    frame.more_data = (frame_control & fc_more_data) != 0;
    frame.protected_frame = (frame_control & fc_protected) != 0;
    frame.order = (frame_control & fc_order) != 0;
}

void read_addresses(const uint8_t *data, Frame & frame)
{
    frame.addr1 = mac_at(data, addr1_offset);
    if (frame.type == Frame::Type::control && has_transmitter(frame) == false)
        return;
    frame.addr2 = mac_at(data, addr2_offset);
    if (frame.type == Frame::Type::control)
        return;

    frame.addr3 = mac_at(data, addr3_offset);
    const uint16_t sequence_control = endian::read_le<uint16_t>(data + sequence_control_offset, 2);
    frame.sequence_number = static_cast<uint16_t>(sequence_control >> sequence_shift);
    frame.fragment_number = static_cast<uint8_t>(sequence_control & fragment_mask);
    if (frame.type == Frame::Type::data && frame.to_ds && frame.from_ds)
        frame.addr4 = mac_at(data, addr4_offset);
}

void read_qos(const uint8_t *data, Frame & frame)
{
    if (is_qos_data(frame) == false)
        return;
    const size_t offset = mac_header_size + ((frame.to_ds && frame.from_ds) ? addr4_size : 0);
    const uint16_t qos_control = endian::read_le<uint16_t>(data + offset, 2);
    frame.qos_tid = static_cast<uint8_t>(qos_control & qos_tid_mask);
    frame.amsdu = (qos_control & qos_amsdu_bit) != 0;
}

Addresses ds_addresses(const Frame & frame)
{
    Addresses result;

    result.ra = frame.addr1;
    result.ta = frame.addr2;

    if (frame.to_ds && frame.from_ds)
    {
        result.da = frame.addr3;
        result.sa = frame.addr4;
        return result;
    }

    if (frame.to_ds)
    {
        result.bssid = frame.addr1;
        result.sa = frame.addr2;
        result.da = frame.addr3;
        return result;
    }

    if (frame.from_ds)
    {
        result.da = frame.addr1;
        result.bssid = frame.addr2;
        result.sa = frame.addr3;
        return result;
    }

    result.da = frame.addr1;
    result.sa = frame.addr2;
    result.bssid = frame.addr3;
    return result;
}

// an A-MSDU carries the real destination and source in each subframe: the outer addr3 and addr4 are not them
Addresses data_addresses(const Frame & frame)
{
    Addresses result = ds_addresses(frame);

    if (frame.amsdu)
    {
        result.da.reset();
        result.sa.reset();
    }
    return result;
}

} // namespace

Result<Frame> parse_frame(ArrUByteView bytes)
{
    if (bytes.size() < control_short_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "802.11 frame needs {} bytes", control_short_size));

    const uint16_t frame_control = endian::read_le<uint16_t>(bytes.data(), 2);
    if ((frame_control & fc_version_mask) != 0)
        return std::unexpected(Error(ErrorCode::invalid_argument,
                                     "802.11 protocol version {} is not supported",
                                     frame_control & fc_version_mask));

    Frame frame;
    frame.type = static_cast<Frame::Type>((frame_control >> fc_type_shift) & fc_type_mask);
    frame.subtype = static_cast<uint8_t>((frame_control >> fc_subtype_shift) & fc_subtype_mask);
    if (frame.type == Frame::Type::extension)
        return std::unexpected(Error(ErrorCode::not_supported, "802.11 extension frames are not decoded"));

    read_flags(frame_control, frame);
    frame.header_size = header_size_of(frame);
    if (bytes.size() < frame.header_size)
        return std::unexpected(Error(ErrorCode::invalid_argument,
                                     "802.11 header needs {} bytes, got {}",
                                     frame.header_size,
                                     bytes.size()));

    frame.duration = endian::read_le<uint16_t>(bytes.data() + 2, 2);
    read_addresses(bytes.data(), frame);
    read_qos(bytes.data(), frame);
    frame.ht_control = has_ht_control(frame);
    frame.body = ArrUByteView(bytes.data() + frame.header_size, bytes.size() - frame.header_size);
    return frame;
}

std::optional<Frame::ManagementSubtype> management_subtype(const Frame & frame)
{
    const bool listed = frame.subtype != management_reserved_subtype_a
                        && frame.subtype != management_reserved_subtype_b;
    if (frame.type != Frame::Type::management || listed == false)
        return std::nullopt;
    return static_cast<Frame::ManagementSubtype>(frame.subtype);
}

std::optional<Frame::ControlSubtype> control_subtype(const Frame & frame)
{
    const bool listed = frame.subtype >= control_first_subtype;
    if (frame.type != Frame::Type::control || listed == false)
        return std::nullopt;
    return static_cast<Frame::ControlSubtype>(frame.subtype);
}

Addresses addresses(const Frame & frame)
{
    if (frame.type == Frame::Type::data)
        return data_addresses(frame);

    Addresses result;
    result.ra = frame.addr1;
    result.ta = frame.addr2;
    if (frame.type == Frame::Type::management)
    {
        result.da = frame.addr1;
        result.sa = frame.addr2;
        result.bssid = frame.addr3;
    }
    else if (frame.type == Frame::Type::control)
    {
        const std::optional<Frame::ControlSubtype> subtype = control_subtype(frame);
        if (subtype == Frame::ControlSubtype::ps_poll)
            result.bssid = frame.addr1;
        else if (subtype == Frame::ControlSubtype::cf_end || subtype == Frame::ControlSubtype::cf_end_ack)
            result.bssid = frame.addr2;
    }
    return result;
}

Result<MacAddress> parse_mac(std::string_view text)
{
    if (text.size() != mac_text_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "a mac address has {} characters", mac_text_size));

    MacAddress mac;
    for (size_t index = 0; index < mac.size(); ++index)
    {
        const char *octet = text.data() + index * mac_octet_text_size;
        const auto [end, error] = std::from_chars(octet, octet + mac_digits_size, mac[index], 16);
        const bool separated = index + 1 == mac.size() || octet[mac_digits_size] == ':';
        if (error != std::errc() || end != octet + mac_digits_size || separated == false)
            return std::unexpected(Error(ErrorCode::invalid_argument, "{} is not a mac address", text));
    }
    return mac;
}

bool is_group(const MacAddress & mac)
{
    return (mac[0] & group_bit) != 0;
}

bool is_locally_administered(const MacAddress & mac)
{
    return (mac[0] & local_bit) != 0;
}

} // namespace sihd::wifi
