#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/frame.hpp>
#include <sihd/wifi/radiotap.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t min_header_size = 8;
constexpr size_t fcs_size = 4;
constexpr uint32_t ext_bit = 1u << 31;
constexpr uint32_t vendor_bit = 1u << 30;
constexpr uint32_t radiotap_bit = 1u << 29;
constexpr uint32_t field_bits_mask = radiotap_bit - 1;

constexpr size_t bit_tsft = 0;
constexpr size_t bit_flags = 1;
constexpr size_t bit_rate = 2;
constexpr size_t bit_channel = 3;
constexpr size_t bit_signal = 5;
constexpr size_t bit_noise = 6;
constexpr size_t bit_antenna = 11;
constexpr size_t bit_mcs = 19;
constexpr size_t bit_xchannel = 18;
constexpr size_t bit_tlv = 28;

struct Layout
{
        size_t align;
        size_t size;
};

// radiotap.org: required alignment and size of each field by present bit, size 0 = cannot be sized
constexpr std::array<Layout, 29> layouts = {{
    {8, 8}, {1, 1},  {1, 1},  {2, 4},  {2, 2},  {1, 1}, {1, 1}, {2, 2}, {2, 2}, {2, 2},
    {1, 1}, {1, 1},  {1, 1},  {1, 1},  {2, 2},  {2, 2}, {1, 1}, {1, 1}, {0, 0}, {1, 3},
    {4, 8}, {2, 12}, {8, 12}, {2, 12}, {2, 12}, {2, 6}, {1, 1}, {2, 4}, {0, 0},
}};

constexpr std::array<size_t, 8> key_bits =
    {bit_tsft, bit_flags, bit_rate, bit_channel, bit_signal, bit_noise, bit_antenna, bit_mcs};

// 802.11n data rates in kb/s for one spatial stream, MCS 0 to 7, long guard interval
constexpr std::array<uint32_t, 8> rate_20mhz = {6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000};
constexpr std::array<uint32_t, 8> rate_40mhz = {13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000};

struct Segment
{
        bool vendor = false;
        uint32_t bits = 0;
        // a second presence word: 0 announces nothing, anything else announces fields nobody can size
        std::optional<uint32_t> extension;
        size_t vendor_data_size = 0;
};

struct Case
{
        Bytes packet;
        Radiotap expected;
        size_t required = 0;
        size_t length = 0;
};

std::optional<uint32_t> mcs_rate(uint8_t index, uint16_t bandwidth, bool guard_known, bool short_gi)
{
    if (index > 31 || bandwidth == 0 || !guard_known)
        return std::nullopt;
    const uint32_t rate = (bandwidth == 40 ? rate_40mhz : rate_20mhz)[index % 8] * (index / 8 + 1u);
    return short_gi ? rate * 10 / 9 : rate;
}

void record_mcs(const Bytes & field, Radiotap & out)
{
    const uint8_t known = field[0];
    const uint8_t flags = field[1];
    if ((known & 0x02) == 0)
        return;

    Radiotap::Mcs mcs;
    mcs.index = field[2];
    if (known & 0x01)
        mcs.bandwidth_mhz = (flags & 0x03) == 1 ? 40 : 20;
    mcs.short_gi = (known & 0x04) && (flags & 0x04);
    mcs.greenfield = (known & 0x08) && (flags & 0x08);
    mcs.ldpc = (known & 0x10) && (flags & 0x10);
    if (known & 0x20)
        mcs.stbc_streams = (flags >> 5) & 0x03;
    mcs.rate_kbps = mcs_rate(mcs.index, mcs.bandwidth_mhz, known & 0x04, mcs.short_gi);
    out.mcs = mcs;
}

void record_field(size_t bit, const Bytes & field, Radiotap & out)
{
    switch (bit)
    {
        case bit_tsft:
            out.tsft_us = test::le_at(field, 0, 8);
            break;
        case bit_flags:
            out.short_preamble = field[0] & 0x02;
            out.wep = field[0] & 0x04;
            out.fragmented = field[0] & 0x08;
            out.fcs_at_end = field[0] & 0x10;
            out.data_padding = field[0] & 0x20;
            out.bad_fcs = field[0] & 0x40;
            break;
        case bit_rate:
            out.rate_kbps = field[0] * 500u;
            break;
        case bit_channel:
            out.frequency_mhz = test::le_at(field, 0, 2);
            out.channel_flags = test::le_at(field, 2, 2);
            break;
        case bit_signal:
            out.signal_dbm = static_cast<int8_t>(field[0]);
            break;
        case bit_noise:
            out.noise_dbm = static_cast<int8_t>(field[0]);
            break;
        case bit_antenna:
            out.antenna = field[0];
            break;
        case bit_mcs:
            record_mcs(field, out);
            break;
        default:
            break;
    }
}

uint8_t consume_byte(FuzzedDataProvider & provider)
{
    return provider.ConsumeIntegral<uint8_t>();
}

void pad_to(Bytes & header, size_t align, FuzzedDataProvider & provider)
{
    while (header.size() % align != 0)
        header.push_back(consume_byte(provider));
}

// returns true when a field that cannot be sized ends the walk
bool emit_fields(Bytes & header, uint32_t bits, Radiotap & target, FuzzedDataProvider & provider)
{
    for (size_t bit = 0; bit < layouts.size(); ++bit)
    {
        if ((bits & (1u << bit)) == 0)
            continue;
        if (layouts[bit].size == 0)
            return true;
        pad_to(header, layouts[bit].align, provider);
        Bytes field(layouts[bit].size);
        for (uint8_t & byte : field)
            byte = consume_byte(provider);
        test::put_bytes(header, field);
        record_field(bit, field, target);
    }
    return false;
}

void emit_vendor(Bytes & header, const Segment & segment, FuzzedDataProvider & provider)
{
    pad_to(header, 2, provider);
    for (size_t i = 0; i < 4; ++i)
        header.push_back(consume_byte(provider));
    test::put_le(header, segment.vendor_data_size, 2);
    for (size_t i = 0; i < segment.vendor_data_size; ++i)
        header.push_back(consume_byte(provider));
}

uint32_t consume_bits(FuzzedDataProvider & provider)
{
    uint32_t bits = provider.ConsumeIntegral<uint32_t>() & provider.ConsumeIntegral<uint32_t>() & field_bits_mask;
    bits &= ~((1u << bit_xchannel) | (1u << bit_tlv));
    const uint8_t wanted = consume_byte(provider);
    for (size_t i = 0; i < key_bits.size(); ++i)
    {
        if (wanted & (1u << i))
            bits |= 1u << key_bits[i];
    }
    if (provider.ConsumeIntegralInRange<int>(0, 7) == 0)
        bits |= 1u << (provider.ConsumeBool() ? bit_xchannel : bit_tlv);
    return bits;
}

Segment consume_segment(FuzzedDataProvider & provider, bool first)
{
    Segment segment;
    segment.vendor = !first && provider.ConsumeIntegralInRange<int>(0, 3) == 0;
    segment.bits = consume_bits(provider);
    if (segment.vendor)
        segment.vendor_data_size = provider.ConsumeIntegralInRange<size_t>(0, 8);
    else if (provider.ConsumeBool())
        segment.extension = provider.ConsumeIntegralInRange<int>(0, 7) == 0
                                ? 1u << provider.ConsumeIntegralInRange<int>(0, 28)
                                : 0;
    return segment;
}

void write_presence(Bytes & header, const std::vector<Segment> & segments)
{
    for (size_t i = 0; i < segments.size(); ++i)
    {
        std::vector<uint32_t> words = {segments[i].bits};
        if (segments[i].extension.has_value())
            words.push_back(*segments[i].extension);
        const bool last_segment = i + 1 == segments.size();
        for (size_t j = 0; j < words.size(); ++j)
        {
            uint32_t word = words[j];
            if (j + 1 < words.size())
                word |= ext_bit;
            else if (!last_segment)
                word |= ext_bit | (segments[i + 1].vendor ? vendor_bit : radiotap_bit);
            test::put_le(header, word, 4);
        }
    }
}

// fills `expected` and `required`: the header size that holds everything the walk reads
void emit_data(Bytes & header, const std::vector<Segment> & segments, Case & made, FuzzedDataProvider & provider)
{
    bool stopped = false;
    for (size_t i = 0; i < segments.size() && !stopped; ++i)
    {
        if (segments[i].vendor)
        {
            emit_vendor(header, segments[i], provider);
            continue;
        }
        Radiotap per_antenna;
        stopped = emit_fields(header, segments[i].bits, i == 0 ? made.expected : per_antenna, provider);
        stopped = stopped || (segments[i].extension.value_or(0) & field_bits_mask) != 0;
        if (i > 0 && per_antenna.antenna.has_value() && per_antenna.signal_dbm.has_value())
            made.expected.antenna_signals.push_back({*per_antenna.antenna, *per_antenna.signal_dbm});
    }
    made.required = header.size();
}

Bytes consume_payload(FuzzedDataProvider & provider)
{
    constexpr std::array<uint16_t, 4> frame_controls = {0x0080, 0x0088, 0x0008, 0x00d4};
    Bytes payload;
    const size_t choice = provider.ConsumeIntegralInRange<size_t>(0, frame_controls.size());
    test::put_le(payload,
                 choice < frame_controls.size() ? frame_controls[choice] : provider.ConsumeIntegral<uint16_t>(),
                 2);
    test::put_bytes(payload, provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 40)));
    return payload;
}

Case build_case(FuzzedDataProvider & provider)
{
    std::vector<Segment> segments(1 + provider.ConsumeIntegralInRange<size_t>(0, 3));
    for (size_t i = 0; i < segments.size(); ++i)
        segments[i] = consume_segment(provider, i == 0);

    Case made;
    Bytes header = {0, 0, 0, 0};
    write_presence(header, segments);
    emit_data(header, segments, made, provider);
    for (size_t slack = provider.ConsumeIntegralInRange<size_t>(0, 3); slack > 0; --slack)
        header.push_back(consume_byte(provider));
    if (provider.ConsumeIntegralInRange<int>(0, 7) == 0)
        header[0] = provider.ConsumeIntegralInRange<uint8_t>(1, 255);

    const Bytes payload = consume_payload(provider);
    made.length = header.size();
    switch (provider.ConsumeIntegralInRange<int>(0, 7))
    {
        case 0:
            made.length = made.required - provider.ConsumeIntegralInRange<size_t>(1, made.required);
            break;
        case 1:
            made.length = header.size() + payload.size() + provider.ConsumeIntegralInRange<size_t>(1, 8);
            break;
        default:
            break;
    }
    header[2] = static_cast<uint8_t>(made.length);
    header[3] = static_cast<uint8_t>(made.length >> 8);

    made.packet = header;
    test::put_bytes(made.packet, payload);
    if (provider.ConsumeIntegralInRange<int>(0, 7) == 0)
        made.packet.resize(made.packet.size() - provider.ConsumeIntegralInRange<size_t>(1, made.packet.size()));
    return made;
}

void check_mcs(const std::optional<Radiotap::Mcs> & got, const std::optional<Radiotap::Mcs> & want)
{
    test::fuzz_check(got.has_value() == want.has_value(), "mcs is present exactly when its index is known");
    if (!got.has_value() || !want.has_value())
        return;
    test::fuzz_check(got->index == want->index, "the mcs index is the third byte");
    test::fuzz_check(got->bandwidth_mhz == want->bandwidth_mhz, "the mcs bandwidth is 20 or 40 when known");
    test::fuzz_check(got->short_gi == want->short_gi, "the short guard interval needs its known bit");
    test::fuzz_check(got->greenfield == want->greenfield, "greenfield needs its known bit");
    test::fuzz_check(got->ldpc == want->ldpc, "ldpc needs its known bit");
    test::fuzz_check(got->stbc_streams == want->stbc_streams, "the stbc streams are bits 5 and 6 when known");
    test::fuzz_check(got->rate_kbps == want->rate_kbps, "the mcs rate follows the 802.11n table");
}

void check_flags(const Radiotap & got, const Radiotap & want)
{
    test::fuzz_check(got.short_preamble == want.short_preamble, "short preamble is flag 0x02");
    test::fuzz_check(got.wep == want.wep, "wep is flag 0x04");
    test::fuzz_check(got.fragmented == want.fragmented, "fragmentation is flag 0x08");
    test::fuzz_check(got.fcs_at_end == want.fcs_at_end, "fcs at end is flag 0x10");
    test::fuzz_check(got.data_padding == want.data_padding, "data padding is flag 0x20");
    test::fuzz_check(got.bad_fcs == want.bad_fcs, "bad fcs is flag 0x40");
}

void check_fields(const Radiotap & got, const Radiotap & want)
{
    test::fuzz_check(got.tsft_us == want.tsft_us, "the tsft is eight little endian bytes");
    test::fuzz_check(got.rate_kbps == want.rate_kbps, "the rate is in 500 kb/s units");
    test::fuzz_check(got.frequency_mhz == want.frequency_mhz, "the frequency is the first channel word");
    test::fuzz_check(got.channel_flags == want.channel_flags, "the channel flags are the second channel word");
    test::fuzz_check(got.signal_dbm == want.signal_dbm, "the signal is a signed byte");
    test::fuzz_check(got.noise_dbm == want.noise_dbm, "the noise is a signed byte");
    test::fuzz_check(got.antenna == want.antenna, "the antenna is one byte");

    check_flags(got, want);
    check_mcs(got.mcs, want.mcs);
    test::fuzz_check(got.antenna_signals.size() == want.antenna_signals.size(), "one entry per antenna namespace");
    for (size_t i = 0; i < want.antenna_signals.size() && i < got.antenna_signals.size(); ++i)
    {
        test::fuzz_check(got.antenna_signals[i].antenna == want.antenna_signals[i].antenna,
                         "an entry keeps its antenna");
        test::fuzz_check(got.antenna_signals[i].signal_dbm == want.antenna_signals[i].signal_dbm,
                         "an entry keeps its signal");
    }
}

void check_dissect(const Bytes & input, const Result<Radiotap> & parsed)
{
    const Result<Packet> packet = dissect_radiotap(ArrUByteView(input));
    if (!parsed.has_value())
    {
        test::fuzz_check(!packet.has_value(), "a refused header is refused by the dissector");
        return;
    }
    const Result<Frame> plain = parse_frame(parsed->payload);
    test::fuzz_check(packet.has_value() == plain.has_value(), "the dissector needs a header and a frame");
    if (!packet.has_value())
        return;

    const Frame & frame = packet->frame;
    const size_t padding = parsed->data_padding ? (4 - plain->header_size % 4) % 4 : 0;
    const size_t skipped = std::min(padding, plain->body.size());
    test::fuzz_check(packet->radiotap.has_value() && packet->radiotap->header_size == parsed->header_size,
                     "the dissector keeps the radiotap header");
    test::fuzz_check(frame.header_size == plain->header_size, "the padding is not part of the frame header");
    test::fuzz_check(frame.body.size() == plain->body.size() - skipped, "the announced padding is skipped");
    test::fuzz_check(frame.body.size() == 0 || frame.body.data() == plain->body.data() + skipped,
                     "the body starts after the padding");
}

void check_case(const Case & made)
{
    const Bytes input = test::exact_copy(made.packet);
    const Result<Radiotap> parsed = parse_radiotap(ArrUByteView(input));
    const bool refused = input.size() < min_header_size || input[0] != 0 || made.length > input.size()
                         || made.length < made.required
                         || (made.expected.fcs_at_end && input.size() - made.length < fcs_size);
    test::fuzz_check(parsed.has_value() != refused, "a header is accepted exactly when its length holds its fields");
    if (!parsed.has_value())
    {
        test::fuzz_check(parsed.error().code == ErrorCode::invalid_argument, "a refused header is invalid_argument");
        check_dissect(input, parsed);
        return;
    }

    check_fields(*parsed, made.expected);
    test::fuzz_check(parsed->header_size == made.length, "the header size is the announced length");
    const size_t payload_size = input.size() - made.length - (made.expected.fcs_at_end ? fcs_size : 0);
    test::fuzz_check(test::is_view_of(parsed->payload, input, made.length, payload_size),
                     "the payload follows the header and drops the fcs");
    check_dissect(input, parsed);
}

void check_raw(const Bytes & raw)
{
    const Bytes input = test::exact_copy(raw);
    const Result<Radiotap> parsed = parse_radiotap(ArrUByteView(input));
    if (parsed.has_value())
    {
        test::fuzz_check(parsed->header_size >= min_header_size && parsed->header_size <= input.size(),
                         "the header size is inside the packet");
        test::fuzz_check(parsed->payload.size() <= input.size() - parsed->header_size,
                         "the payload is inside the packet");
        test::fuzz_check(parsed->payload.size() == 0 || parsed->payload.data() == input.data() + parsed->header_size,
                         "the payload starts after the header");
    }
    check_dissect(input, parsed);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        check_case(build_case(provider));
    else
        check_raw(provider.ConsumeRemainingBytes<uint8_t>());
    return 0;
}
