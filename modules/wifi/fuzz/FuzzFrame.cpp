#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <optional>
#include <string>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/frame.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t short_control_size = 10;
constexpr size_t mac_text_size = 17;
constexpr uint8_t subtype_control_wrapper = 7;
constexpr uint8_t subtype_cts = 12;
constexpr uint8_t subtype_ack = 13;
constexpr uint8_t subtype_ps_poll = 10;
constexpr uint8_t subtype_cf_end = 14;
constexpr uint8_t subtype_cf_end_ack = 15;
constexpr uint8_t subtype_qos_bit = 0x08;

struct Case
{
        Bytes bytes;
        Frame want;
        uint8_t version = 0;
};

bool has_transmitter(const Frame & frame)
{
    const bool short_control = frame.subtype == subtype_cts || frame.subtype == subtype_ack;
    return frame.type != Frame::Type::control || (!short_control && frame.subtype != subtype_control_wrapper);
}

bool has_qos(const Frame & frame)
{
    return frame.type == Frame::Type::data && (frame.subtype & subtype_qos_bit) != 0;
}

bool has_addr4(const Frame & frame)
{
    return frame.type == Frame::Type::data && frame.to_ds && frame.from_ds;
}

bool has_ht_control(const Frame & frame)
{
    return frame.order && (frame.type == Frame::Type::management || has_qos(frame));
}

MacAddress consume_mac(FuzzedDataProvider & provider)
{
    MacAddress mac;
    for (uint8_t & byte : mac)
        byte = provider.ConsumeIntegral<uint8_t>();
    return mac;
}

void put_mac(Bytes & out, const MacAddress & mac)
{
    out.insert(out.end(), mac.begin(), mac.end());
}

void consume_flags(FuzzedDataProvider & provider, Frame & frame)
{
    const uint8_t flags = provider.ConsumeIntegral<uint8_t>();
    frame.to_ds = flags & 0x01;
    frame.from_ds = flags & 0x02;
    frame.more_fragments = flags & 0x04;
    frame.retry = flags & 0x08;
    frame.power_management = flags & 0x10;
    frame.more_data = flags & 0x20;
    frame.protected_frame = flags & 0x40;
    frame.order = flags & 0x80;
}

Frame consume_frame(FuzzedDataProvider & provider)
{
    Frame frame;
    frame.type = static_cast<Frame::Type>(provider.ConsumeIntegralInRange<int>(0, 2));
    frame.subtype = provider.ConsumeIntegralInRange<uint8_t>(frame.type == Frame::Type::control ? 2 : 0, 15);
    consume_flags(provider, frame);
    frame.duration = provider.ConsumeIntegral<uint16_t>();

    frame.addr1 = consume_mac(provider);
    if (has_transmitter(frame))
        frame.addr2 = consume_mac(provider);
    if (frame.type != Frame::Type::control)
    {
        frame.addr3 = consume_mac(provider);
        const uint16_t sequence_control = provider.ConsumeIntegral<uint16_t>();
        frame.sequence_number = sequence_control >> 4;
        frame.fragment_number = sequence_control & 0x0f;
    }
    if (has_addr4(frame))
        frame.addr4 = consume_mac(provider);

    if (has_qos(frame))
    {
        const uint16_t qos_control = provider.ConsumeIntegral<uint16_t>();
        frame.qos_tid = qos_control & 0x0f;
        frame.amsdu = qos_control & 0x80;
    }
    frame.ht_control = has_ht_control(frame);
    return frame;
}

uint16_t sequence_control_of(const Frame & frame)
{
    return static_cast<uint16_t>((*frame.sequence_number << 4) | *frame.fragment_number);
}

uint16_t qos_control_of(const Frame & frame, FuzzedDataProvider & provider)
{
    const uint16_t high_bits = provider.ConsumeIntegral<uint16_t>() & 0xff70;
    return static_cast<uint16_t>(high_bits | *frame.qos_tid | (frame.amsdu ? 0x80 : 0));
}

Bytes encode_header(Frame & frame, FuzzedDataProvider & provider)
{
    const uint16_t frame_control = static_cast<uint16_t>(
        (static_cast<unsigned>(frame.type) << 2) | (frame.subtype << 4) | (frame.to_ds ? 0x0100 : 0)
        | (frame.from_ds ? 0x0200 : 0) | (frame.more_fragments ? 0x0400 : 0) | (frame.retry ? 0x0800 : 0)
        | (frame.power_management ? 0x1000 : 0) | (frame.more_data ? 0x2000 : 0) | (frame.protected_frame ? 0x4000 : 0)
        | (frame.order ? 0x8000 : 0));
    Bytes out;
    test::put_le(out, frame_control, 2);
    test::put_le(out, frame.duration, 2);

    put_mac(out, frame.addr1);
    if (frame.addr2.has_value())
        put_mac(out, *frame.addr2);
    if (frame.type == Frame::Type::control && frame.subtype == subtype_control_wrapper)
        test::put_le(out, provider.ConsumeIntegral<uint64_t>(), 6);
    if (frame.addr3.has_value())
    {
        put_mac(out, *frame.addr3);
        test::put_le(out, sequence_control_of(frame), 2);
    }
    if (frame.addr4.has_value())
        put_mac(out, *frame.addr4);

    if (has_qos(frame))
        test::put_le(out, qos_control_of(frame, provider), 2);
    if (frame.ht_control)
        test::put_le(out, provider.ConsumeIntegral<uint32_t>(), 4);
    frame.header_size = out.size();
    return out;
}

Case build_case(FuzzedDataProvider & provider)
{
    Case made;
    made.want = consume_frame(provider);
    Bytes bytes = encode_header(made.want, provider);
    const Bytes body = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 12));
    test::put_bytes(bytes, body);
    if (provider.ConsumeIntegralInRange<int>(0, 7) == 0)
    {
        made.version = provider.ConsumeIntegralInRange<uint8_t>(1, 3);
        bytes[0] |= made.version;
    }
    made.bytes = bytes;
    return made;
}

void downlink_roles(const Frame & frame, Addresses & roles)
{
    if (frame.from_ds)
    {
        roles.da = frame.addr1;
        roles.bssid = frame.addr2;
        roles.sa = frame.addr3;
        return;
    }
    roles.da = frame.addr1;
    roles.sa = frame.addr2;
    roles.bssid = frame.addr3;
}

void ds_roles(const Frame & frame, Addresses & roles)
{
    if (frame.to_ds && frame.from_ds)
    {
        roles.da = frame.addr3;
        roles.sa = frame.addr4;
    }
    else if (frame.to_ds)
    {
        roles.bssid = frame.addr1;
        roles.sa = frame.addr2;
        roles.da = frame.addr3;
    }
    else
        downlink_roles(frame, roles);
}

void data_roles(const Frame & frame, Addresses & roles)
{
    ds_roles(frame, roles);
    if (frame.amsdu)
    {
        roles.da.reset();
        roles.sa.reset();
    }
}

Addresses roles_of(const Frame & frame)
{
    Addresses roles;
    roles.ra = frame.addr1;
    roles.ta = frame.addr2;
    if (frame.type == Frame::Type::management)
    {
        roles.da = frame.addr1;
        roles.sa = frame.addr2;
        roles.bssid = frame.addr3;
    }
    else if (frame.type == Frame::Type::control && frame.subtype == subtype_ps_poll)
        roles.bssid = frame.addr1;
    else if (frame.type == Frame::Type::control
             && (frame.subtype == subtype_cf_end || frame.subtype == subtype_cf_end_ack))
        roles.bssid = frame.addr2;
    else if (frame.type == Frame::Type::data)
        data_roles(frame, roles);
    return roles;
}

void check_subtypes(const Frame & got, const Frame & want)
{
    const bool management_listed = want.subtype != 7 && want.subtype != 15;
    const bool management = want.type == Frame::Type::management && management_listed;
    test::fuzz_check(management_subtype(got).has_value() == management, "a management subtype is listed but 7 and 15");
    if (management)
        test::fuzz_check(static_cast<uint8_t>(*management_subtype(got)) == want.subtype,
                         "the management subtype is kept");

    const bool control = want.type == Frame::Type::control && want.subtype >= 2;
    test::fuzz_check(control_subtype(got).has_value() == control, "a control subtype is listed from 2");
    if (control)
        test::fuzz_check(static_cast<uint8_t>(*control_subtype(got)) == want.subtype, "the control subtype is kept");
}

void check_roles(const Frame & got, const Frame & want)
{
    const Addresses roles = addresses(got);
    const Addresses expected = roles_of(want);
    test::fuzz_check(roles.ra == expected.ra, "the receiver is the first address");
    test::fuzz_check(roles.ta == expected.ta, "the transmitter is the second address when there is one");
    test::fuzz_check(roles.bssid == expected.bssid, "the bssid comes from the ds bits and the frame kind");
    test::fuzz_check(roles.sa == expected.sa, "the source comes from the ds bits and the frame kind");
    test::fuzz_check(roles.da == expected.da, "the destination comes from the ds bits and the frame kind");
}

void check_flags(const Frame & got, const Frame & want)
{
    test::fuzz_check(got.type == want.type && got.subtype == want.subtype, "the type and subtype are in frame control");
    test::fuzz_check(got.to_ds == want.to_ds && got.from_ds == want.from_ds,
                     "the ds bits are frame control bits 8 and 9");
    test::fuzz_check(got.more_fragments == want.more_fragments, "more fragments is frame control bit 10");
    test::fuzz_check(got.retry == want.retry, "retry is frame control bit 11");
    test::fuzz_check(got.power_management == want.power_management, "power management is frame control bit 12");
    test::fuzz_check(got.more_data == want.more_data, "more data is frame control bit 13");
    test::fuzz_check(got.protected_frame == want.protected_frame, "protected is frame control bit 14");
    test::fuzz_check(got.order == want.order, "order is frame control bit 15");
}

void check_header(const Frame & got, const Frame & want)
{
    test::fuzz_check(got.duration == want.duration, "the duration follows frame control");
    test::fuzz_check(got.addr1 == want.addr1 && got.addr2 == want.addr2,
                     "the first two addresses are where they belong");
    test::fuzz_check(got.addr3 == want.addr3 && got.addr4 == want.addr4,
                     "the last two addresses are where they belong");

    test::fuzz_check(got.sequence_number == want.sequence_number, "the sequence number is the top 12 bits");
    test::fuzz_check(got.fragment_number == want.fragment_number, "the fragment number is the low 4 bits");
    test::fuzz_check(got.qos_tid == want.qos_tid, "the tid is the low 4 bits of the qos control");
    test::fuzz_check(got.amsdu == want.amsdu, "a-msdu is bit 7 of the qos control");
    test::fuzz_check(got.ht_control == want.ht_control, "an ht control field comes with the order bit");
}

void check_frame(const Frame & got, const Frame & want, const Bytes & input)
{
    check_flags(got, want);
    check_header(got, want);

    test::fuzz_check(got.header_size == want.header_size, "the header size follows the frame kind");
    test::fuzz_check(test::is_view_of(got.body, input, want.header_size, input.size() - want.header_size),
                     "the body is what follows the header");

    check_subtypes(got, want);
    check_roles(got, want);
}

void check_case(const Case & made)
{
    const Bytes input = test::exact_copy(made.bytes);
    const Result<Frame> frame = parse_frame(ArrUByteView(input));
    test::fuzz_check(frame.has_value() == (made.version == 0), "only protocol version 0 is decoded");
    if (!frame.has_value())
    {
        test::fuzz_check(frame.error().code == ErrorCode::invalid_argument, "a wrong version is invalid_argument");
        return;
    }
    check_frame(*frame, made.want, input);

    const Result<Packet> packet = dissect_80211(ArrUByteView(input));
    test::fuzz_check(packet.has_value() && !packet->radiotap.has_value(), "the bare dissector has no radiotap");
    test::fuzz_check(packet->frame.header_size == frame->header_size, "the bare dissector decodes the same frame");

    const Bytes cut = test::exact_copy(Bytes(made.bytes.begin(), made.bytes.begin() + made.want.header_size - 1));
    const Result<Frame> shorter = parse_frame(ArrUByteView(cut));
    test::fuzz_check(!shorter.has_value() && shorter.error().code == ErrorCode::invalid_argument,
                     "a header one byte short is invalid_argument");
}

void check_extension(FuzzedDataProvider & provider)
{
    Bytes bytes = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 16));
    if (!bytes.empty())
        bytes[0] = static_cast<uint8_t>((bytes[0] & 0xf0) | 0x0c);
    const Bytes input = test::exact_copy(bytes);
    const Result<Frame> frame = parse_frame(ArrUByteView(input));
    test::fuzz_check(!frame.has_value(), "an extension frame is not decoded");
    const ErrorCode expected = input.size() < short_control_size ? ErrorCode::invalid_argument
                                                                 : ErrorCode::not_supported;
    test::fuzz_check(frame.error().code == expected, "an extension frame is not_supported once it is long enough");
}

void check_raw(const Bytes & raw)
{
    const Bytes input = test::exact_copy(raw);
    const Result<Frame> frame = parse_frame(ArrUByteView(input));
    const Result<Packet> packet = dissect_80211(ArrUByteView(input));
    test::fuzz_check(frame.has_value() == packet.has_value(), "the bare dissector accepts what the parser accepts");
    if (!frame.has_value())
        return;
    test::fuzz_check(frame->header_size >= short_control_size && frame->header_size <= input.size(),
                     "the header size is inside the frame");
    test::fuzz_check(test::is_view_of(frame->body, input, frame->header_size, input.size() - frame->header_size),
                     "the body is what follows the header");
    test::fuzz_check(frame->type != Frame::Type::extension, "an extension frame is not decoded");
}

std::string format_mac(const MacAddress & mac, bool upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string text;
    for (const uint8_t byte : mac)
    {
        if (!text.empty())
            text.push_back(':');
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0f]);
    }
    return text;
}

void check_mac_text(FuzzedDataProvider & provider)
{
    const MacAddress mac = consume_mac(provider);
    std::string text = format_mac(mac, provider.ConsumeBool());
    const int damage = provider.ConsumeIntegralInRange<int>(0, 4);
    const size_t at = provider.ConsumeIntegralInRange<size_t>(0, mac_text_size - 1);
    switch (damage)
    {
        case 1:
            text.pop_back();
            break;
        case 2:
            text.push_back('0');
            break;
        case 3:
            text[at] = at % 3 == 2 ? "-. ;"[at % 4] : "g+ x"[at % 4];
            break;
        default:
            break;
    }

    const test::ExactText exact(text);
    const Result<MacAddress> parsed = parse_mac(exact);
    const bool damaged = damage == 1 || damage == 2 || damage == 3;
    test::fuzz_check(parsed.has_value() != damaged, "a mac is accepted exactly when it is six hex pairs with colons");
    if (!parsed.has_value())
        return;
    test::fuzz_check(*parsed == mac, "the octets are the hex pairs");
    test::fuzz_check(is_group(mac) == ((mac[0] & 0x01) != 0), "the group bit is bit 0 of the first octet");
    test::fuzz_check(is_locally_administered(mac) == ((mac[0] & 0x02) != 0),
                     "the local bit is bit 1 of the first octet");
}

void check_mac_raw(FuzzedDataProvider & provider)
{
    const test::ExactText text(provider.ConsumeRandomLengthString(24));
    const Result<MacAddress> parsed = parse_mac(text);
    if (!parsed.has_value())
        return;
    std::string lower(text.view());
    std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return c >= 'A' && c <= 'F' ? c + 32 : c; });
    test::fuzz_check(format_mac(*parsed, false) == lower, "an accepted mac is its own text");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 4))
    {
        case 0:
            check_mac_raw(provider);
            check_raw(provider.ConsumeRemainingBytes<uint8_t>());
            break;
        case 1:
            check_mac_text(provider);
            break;
        case 2:
            check_extension(provider);
            break;
        default:
            check_case(build_case(provider));
            break;
    }
    return 0;
}
