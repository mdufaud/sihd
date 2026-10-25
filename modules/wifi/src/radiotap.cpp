#include <array>

#include <sihd/util/endian.hpp>
#include <sihd/wifi/radiotap.hpp>

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr uint8_t version = 0;
constexpr size_t min_header_size = 8;
constexpr size_t length_offset = 2;
constexpr size_t presence_offset = 4;
constexpr size_t presence_size = 4;
constexpr size_t vendor_header_size = 6;
constexpr size_t vendor_skip_length_offset = 4;
constexpr size_t vendor_alignment = 2;
constexpr size_t fcs_size = 4;
constexpr uint32_t rate_unit_kbps = 500;

constexpr uint32_t present_radiotap_namespace = 1u << 29;
constexpr uint32_t present_vendor_namespace = 1u << 30;
constexpr uint32_t present_extension = 1u << 31;
constexpr size_t present_field_bits = 29;

constexpr size_t bit_tsft = 0;
constexpr size_t bit_flags = 1;
constexpr size_t bit_rate = 2;
constexpr size_t bit_channel = 3;
constexpr size_t bit_signal = 5;
constexpr size_t bit_noise = 6;
constexpr size_t bit_antenna = 11;
constexpr size_t bit_mcs = 19;

constexpr size_t mcs_known_offset = 0;
constexpr size_t mcs_flags_offset = 1;
constexpr size_t mcs_index_offset = 2;

constexpr uint8_t mcs_known_bandwidth = 0x01;
constexpr uint8_t mcs_known_index = 0x02;
constexpr uint8_t mcs_known_guard_interval = 0x04;
constexpr uint8_t mcs_known_format = 0x08;
constexpr uint8_t mcs_known_fec = 0x10;
constexpr uint8_t mcs_known_stbc = 0x20;

constexpr uint8_t mcs_flag_bandwidth_mask = 0x03;
constexpr uint8_t mcs_flag_bandwidth_40 = 0x01;
constexpr uint8_t mcs_flag_short_gi = 0x04;
constexpr uint8_t mcs_flag_greenfield = 0x08;
constexpr uint8_t mcs_flag_ldpc = 0x10;
constexpr uint8_t mcs_flag_stbc_mask = 0x60;
constexpr unsigned mcs_flag_stbc_shift = 5;

constexpr uint8_t mcs_highest_rated_index = 31;
constexpr size_t mcs_streams_per_step = 8;

constexpr uint32_t short_gi_numerator = 10;
constexpr uint32_t short_gi_denominator = 9;

// data rate of one spatial stream in kb/s by MCS index modulo 8, at 20 and at 40 MHz, long guard interval
constexpr std::array<uint32_t, 8> rate_20mhz_kbps = {6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000};
constexpr std::array<uint32_t, 8> rate_40mhz_kbps = {13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000};

constexpr uint8_t flag_short_preamble = 0x02;
constexpr uint8_t flag_wep = 0x04;
constexpr uint8_t flag_fragmented = 0x08;
constexpr uint8_t flag_fcs_at_end = 0x10;
constexpr uint8_t flag_data_padding = 0x20;
constexpr uint8_t flag_bad_fcs = 0x40;

enum class Namespace
{
    radiotap,
    vendor
};

struct Layout
{
        size_t align;
        size_t size;
};

// size and alignment of each field by present bit, size 0 = undefined by the spec (docs/protocols/wifi-frames.md)
constexpr std::array<Layout, 28> field_layouts = {{
    {8, 8},  // 0 TSFT
    {1, 1},  // 1 flags
    {1, 1},  // 2 rate
    {2, 4},  // 3 channel
    {2, 2},  // 4 FHSS
    {1, 1},  // 5 antenna signal dBm
    {1, 1},  // 6 antenna noise dBm
    {2, 2},  // 7 lock quality
    {2, 2},  // 8 TX attenuation
    {2, 2},  // 9 dB TX attenuation
    {1, 1},  // 10 TX power dBm
    {1, 1},  // 11 antenna
    {1, 1},  // 12 antenna signal dB
    {1, 1},  // 13 antenna noise dB
    {2, 2},  // 14 RX flags
    {2, 2},  // 15 TX flags
    {1, 1},  // 16 RTS retries
    {1, 1},  // 17 data retries
    {0, 0},  // 18 XChannel, not in the official list
    {1, 3},  // 19 MCS
    {4, 8},  // 20 A-MPDU status
    {2, 12}, // 21 VHT
    {8, 12}, // 22 timestamp
    {2, 12}, // 23 HE
    {2, 12}, // 24 HE-MU
    {2, 6},  // 25 HE-MU other user
    {1, 1},  // 26 zero-length PSDU
    {2, 4},  // 27 L-SIG
}};

// next multiple of `align` at or after `position`; positions count from the first byte of the radiotap header
size_t align_up(size_t position, size_t align)
{
    return (position + align - 1) / align * align;
}

void apply_flags(uint8_t flags, Radiotap & radiotap)
{
    radiotap.short_preamble = (flags & flag_short_preamble) != 0;
    radiotap.wep = (flags & flag_wep) != 0;
    radiotap.fragmented = (flags & flag_fragmented) != 0;
    radiotap.fcs_at_end = (flags & flag_fcs_at_end) != 0;
    radiotap.data_padding = (flags & flag_data_padding) != 0;
    radiotap.bad_fcs = (flags & flag_bad_fcs) != 0;
}

// the rate needs a known bandwidth, index and guard interval; MCS 32 and above (duplicate and unequal) are left out
std::optional<uint32_t> mcs_rate_kbps(const Radiotap::Mcs & mcs, bool guard_interval_known)
{
    if (mcs.index > mcs_highest_rated_index || mcs.bandwidth_mhz == 0 || guard_interval_known == false)
        return std::nullopt;
    const std::array<uint32_t, 8> & per_stream = mcs.bandwidth_mhz == 40 ? rate_40mhz_kbps : rate_20mhz_kbps;
    const uint32_t streams = static_cast<uint32_t>(mcs.index / mcs_streams_per_step) + 1;
    const uint32_t rate = per_stream[mcs.index % mcs_streams_per_step] * streams;
    return mcs.short_gi ? rate * short_gi_numerator / short_gi_denominator : rate;
}

void store_mcs(const uint8_t *field, Radiotap & radiotap)
{
    const uint8_t known = field[mcs_known_offset];
    const uint8_t flags = field[mcs_flags_offset];
    if ((known & mcs_known_index) == 0)
        return;

    Radiotap::Mcs mcs;
    mcs.index = field[mcs_index_offset];
    if ((known & mcs_known_bandwidth) != 0)
        mcs.bandwidth_mhz = (flags & mcs_flag_bandwidth_mask) == mcs_flag_bandwidth_40 ? 40 : 20;
    mcs.short_gi = (known & mcs_known_guard_interval) != 0 && (flags & mcs_flag_short_gi) != 0;
    mcs.greenfield = (known & mcs_known_format) != 0 && (flags & mcs_flag_greenfield) != 0;
    mcs.ldpc = (known & mcs_known_fec) != 0 && (flags & mcs_flag_ldpc) != 0;
    if ((known & mcs_known_stbc) != 0)
        mcs.stbc_streams = static_cast<uint8_t>((flags & mcs_flag_stbc_mask) >> mcs_flag_stbc_shift);
    mcs.rate_kbps = mcs_rate_kbps(mcs, (known & mcs_known_guard_interval) != 0);
    radiotap.mcs = mcs;
}

void store_field(const uint8_t *field, size_t bit, Radiotap & radiotap)
{
    switch (bit)
    {
        case bit_tsft:
            radiotap.tsft_us = endian::read_le<uint64_t>(field, 8);
            break;
        case bit_flags:
            apply_flags(field[0], radiotap);
            break;
        case bit_rate:
            radiotap.rate_kbps = static_cast<uint32_t>(field[0]) * rate_unit_kbps;
            break;
        case bit_channel:
            radiotap.frequency_mhz = endian::read_le<uint16_t>(field, 2);
            radiotap.channel_flags = endian::read_le<uint16_t>(field + 2, 2);
            break;
        case bit_signal:
            radiotap.signal_dbm = static_cast<int8_t>(field[0]);
            break;
        case bit_noise:
            radiotap.noise_dbm = static_cast<int8_t>(field[0]);
            break;
        case bit_antenna:
            radiotap.antenna = field[0];
            break;
        case bit_mcs:
            store_mcs(field, radiotap);
            break;
        default:
            break;
    }
}

Result<bool> read_fields(ArrUByteView header, uint32_t word, size_t & position, Radiotap & radiotap)
{
    for (size_t bit = 0; bit < present_field_bits; ++bit)
    {
        if ((word & (1u << bit)) == 0)
            continue;
        if (bit >= field_layouts.size() || field_layouts[bit].size == 0)
            return false;
        const Layout & layout = field_layouts[bit];
        position = align_up(position, layout.align);
        if (position + layout.size > header.size())
            return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap field {} runs past the header", bit));
        store_field(header.data() + position, bit, radiotap);
        position += layout.size;
    }
    return true;
}

Result<size_t> presence_end_of(ArrUByteView header)
{
    size_t end = presence_offset;
    bool more = true;
    while (more)
    {
        if (end + presence_size > header.size())
            return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap presence words run past the header"));
        more = (endian::read_le<uint32_t>(header.data() + end, presence_size) & present_extension) != 0;
        end += presence_size;
    }
    return end;
}

Result<void> skip_vendor_namespace(ArrUByteView header, size_t & position)
{
    position = align_up(position, vendor_alignment);
    if (position + vendor_header_size > header.size())
        return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap vendor namespace runs past the header"));
    const size_t skip = endian::read_le<uint16_t>(header.data() + position + vendor_skip_length_offset, 2);
    position += vendor_header_size + skip;
    if (position > header.size())
        return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap vendor data runs past the header"));
    return {};
}

Result<void> walk_presence(ArrUByteView header, size_t presence_end, Radiotap & radiotap)
{
    // `position` walks the data area after the last present word, namespaces: docs/protocols/wifi-frames.md
    size_t position = presence_end;
    Namespace current = Namespace::radiotap;
    size_t word_in_namespace = 0;
    size_t radiotap_namespaces = 0;

    for (size_t offset = presence_offset; offset < presence_end; offset += presence_size)
    {
        const uint32_t word = endian::read_le<uint32_t>(header.data() + offset, presence_size);

        if (current == Namespace::radiotap && word_in_namespace > 0)
        {
            // fields defined by radiotap.org but not decoded: the walk stops
            if ((word & ((1u << present_field_bits) - 1)) != 0)
                return {};
        }
        else if (current == Namespace::radiotap)
        {
            // the first namespace is the frame as a whole, the following ones are one antenna each
            Radiotap antenna;
            Radiotap & target = radiotap_namespaces == 0 ? radiotap : antenna;
            Result<bool> walk_continues = read_fields(header, word, position, target);
            SIHD_UNEXPECTED_RETURN(walk_continues);
            if (antenna.antenna.has_value() && antenna.signal_dbm.has_value())
                radiotap.antenna_signals.push_back({*antenna.antenna, *antenna.signal_dbm});
            ++radiotap_namespaces;
            if (*walk_continues == false)
                return {};
        }

        if ((word & present_vendor_namespace) != 0)
        {
            Result<void> skipped = skip_vendor_namespace(header, position);
            SIHD_UNEXPECTED_RETURN(skipped);
            current = Namespace::vendor;
            word_in_namespace = 0;
        }
        else if ((word & present_radiotap_namespace) != 0)
        {
            current = Namespace::radiotap;
            word_in_namespace = 0;
        }
        else
            ++word_in_namespace;
    }
    return {};
}

} // namespace

Result<Radiotap> parse_radiotap(ArrUByteView bytes)
{
    if (bytes.size() < min_header_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap header needs {} bytes", min_header_size));
    if (bytes[0] != version)
        return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap version {} is not supported", bytes[0]));
    const size_t length = endian::read_le<uint16_t>(bytes.data() + length_offset, 2);
    if (length < min_header_size || length > bytes.size())
        return std::unexpected(
            Error(ErrorCode::invalid_argument, "radiotap length {} does not fit {} bytes", length, bytes.size()));

    const ArrUByteView header(bytes.data(), length);
    Result<size_t> presence_end = presence_end_of(header);
    SIHD_UNEXPECTED_RETURN(presence_end);

    Radiotap radiotap;
    radiotap.header_size = length;
    Result<void> walked = walk_presence(header, *presence_end, radiotap);
    SIHD_UNEXPECTED_RETURN(walked);

    size_t payload_size = bytes.size() - length;
    if (radiotap.fcs_at_end)
    {
        if (payload_size < fcs_size)
            return std::unexpected(Error(ErrorCode::invalid_argument, "radiotap frame is shorter than its fcs"));
        payload_size -= fcs_size;
    }
    radiotap.payload = ArrUByteView(bytes.data() + length, payload_size);
    return radiotap;
}

} // namespace sihd::wifi
