#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/pcap/PcapReader.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Timestamp.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::pcap;
using namespace sihd::util;

namespace
{
constexpr uint32_t magic_micro = 0xa1b2c3d4;
constexpr uint32_t magic_nano = 0xa1b23c4d;
constexpr uint32_t linktype_ethernet = 1;
constexpr int64_t nanos_per_second = 1'000'000'000;
constexpr int64_t nanos_per_micro = 1'000;

enum Precision
{
    file_default,
    micro,
    nano,
};

struct Packet
{
        uint32_t seconds;
        uint32_t fraction;
        uint32_t original_size;
        std::string data;
};

Result<void> open_reader(PcapReader & reader, FILE *stream, int precision)
{
    switch (precision)
    {
        case micro:
            return reader.open_micro_precision(stream);
        case nano:
            return reader.open_nano_precision(stream);
        default:
            return reader.open(stream);
    }
}

void fuzz_raw(FuzzedDataProvider & provider)
{
    const int precision = provider.ConsumeIntegralInRange<int>(file_default, nano);
    std::vector<char> bytes = provider.ConsumeRemainingBytes<char>();
    if (bytes.empty())
        return;

    FILE *stream = fmemopen(bytes.data(), bytes.size(), "r");
    if (stream == nullptr)
        return;

    PcapReader reader("pcap-fuzz");
    if (!open_reader(reader, stream, precision).has_value())
    {
        // a refused stream stays open, an accepted one is closed with the reader
        fclose(stream);
        return;
    }
    test::fuzz_check(reader.file() == stream, "the reader reads the stream it was given");
    (void)reader.snaplen();
    (void)reader.datalink();
    (void)reader.major_version();
    (void)reader.minor_version();
    (void)reader.is_swapped();

    size_t packets = 0;
    while (true)
    {
        const Result<bool> next = reader.read_next();
        if (!next.has_value() || !next.value())
            break;
        ++packets;
        test::fuzz_check(packets <= bytes.size(), "each packet consumes at least one byte of the stream");
        test::fuzz_check(reader.packet_caplen() <= bytes.size(), "a packet cannot hold more bytes than the stream");

        ArrCharView view;
        if (reader.get_read_data(view))
            test::fuzz_check(view.size() == reader.packet_caplen(), "the packet data is the captured bytes");
        (void)reader.get_read_timestamp();
    }
}

void put_u16(std::string & bytes, uint16_t value, bool big_endian)
{
    const uint8_t low = static_cast<uint8_t>(value);
    const uint8_t high = static_cast<uint8_t>(value >> 8);
    bytes += static_cast<char>(big_endian ? high : low);
    bytes += static_cast<char>(big_endian ? low : high);
}

void put_u32(std::string & bytes, uint32_t value, bool big_endian)
{
    const uint16_t low = static_cast<uint16_t>(value);
    const uint16_t high = static_cast<uint16_t>(value >> 16);
    put_u16(bytes, big_endian ? high : low, big_endian);
    put_u16(bytes, big_endian ? low : high, big_endian);
}

std::vector<Packet> consume_packets(FuzzedDataProvider & provider, uint32_t snaplen, bool file_nano)
{
    std::vector<Packet> packets(provider.ConsumeIntegralInRange<size_t>(0, 6));
    for (Packet & packet : packets)
    {
        packet.seconds = provider.ConsumeIntegral<uint32_t>();
        packet.fraction = provider.ConsumeIntegralInRange<uint32_t>(0, file_nano ? 999'999'999 : 999'999);
        packet.data = provider.ConsumeBytesAsString(provider.ConsumeIntegralInRange<size_t>(0, snaplen));
        // over the captured size: the capture was cut by the snaplen
        packet.original_size = static_cast<uint32_t>(packet.data.size())
                               + provider.ConsumeIntegralInRange<uint32_t>(0, 64);
    }
    return packets;
}

std::string encode_capture(const std::vector<Packet> & packets, bool big_endian, bool file_nano, uint32_t snaplen)
{
    std::string bytes;
    put_u32(bytes, file_nano ? magic_nano : magic_micro, big_endian);
    put_u16(bytes, 2, big_endian);
    put_u16(bytes, 4, big_endian);
    put_u32(bytes, 0, big_endian);
    put_u32(bytes, 0, big_endian);
    put_u32(bytes, snaplen, big_endian);
    put_u32(bytes, linktype_ethernet, big_endian);

    for (const Packet & packet : packets)
    {
        put_u32(bytes, packet.seconds, big_endian);
        put_u32(bytes, packet.fraction, big_endian);
        put_u32(bytes, static_cast<uint32_t>(packet.data.size()), big_endian);
        put_u32(bytes, packet.original_size, big_endian);
        bytes += packet.data;
    }
    return bytes;
}

int64_t expected_nanos(const Packet & packet, bool file_nano, bool read_nano)
{
    int64_t fraction_nanos = file_nano ? packet.fraction : packet.fraction * nanos_per_micro;
    // a micro reader drops the nanoseconds of a nano file
    if (!read_nano)
        fraction_nanos = fraction_nanos / nanos_per_micro * nanos_per_micro;
    return static_cast<int64_t>(packet.seconds) * nanos_per_second + fraction_nanos;
}

void check_no_packet_yet(PcapReader & reader)
{
    ArrCharView view;
    test::fuzz_check(!reader.get_read_data(view), "there is no data before the first read");
    test::fuzz_check(!reader.get_read_timestamp().has_value(), "there is no timestamp before the first read");
    test::fuzz_check(reader.packet_len() == 0 && reader.packet_caplen() == 0, "sizes are zero before the first read");
}

void check_packet(PcapReader & reader, const Packet & packet, bool file_nano, bool read_nano)
{
    const Result<bool> next = reader.read_next();
    test::fuzz_check(next.has_value() && *next, "every encoded packet is read");
    test::fuzz_check(reader.packet_caplen() == packet.data.size(), "packet_caplen is the captured size");
    test::fuzz_check(reader.packet_len() == packet.original_size, "packet_len is the original size");

    ArrCharView view;
    test::fuzz_check(reader.get_read_data(view), "a read packet exposes its data");
    test::fuzz_check(std::string_view(view.data(), view.size()) == packet.data, "the data is the captured bytes");

    const Result<Timestamp> timestamp = reader.get_read_timestamp();
    test::fuzz_check(timestamp.has_value(), "a read packet has a timestamp");
    test::fuzz_check(timestamp->get() == expected_nanos(packet, file_nano, read_nano),
                     "the timestamp follows the precision of the file and of the reader");
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    const bool big_endian = provider.ConsumeBool();
    const bool file_nano = provider.ConsumeBool();
    const int precision = provider.ConsumeIntegralInRange<int>(file_default, nano);
    const uint32_t snaplen = provider.ConsumeIntegralInRange<uint32_t>(64, 65535);
    const std::vector<Packet> packets = consume_packets(provider, 64, file_nano);
    std::string bytes = encode_capture(packets, big_endian, file_nano, snaplen);

    PcapReader reader("pcap-fuzz");
    test::fuzz_check(!reader.read_next().has_value(), "a closed reader refuses to read");

    FILE *stream = fmemopen(bytes.data(), bytes.size(), "r");
    test::fuzz_check(stream != nullptr, "an in-memory stream opens");
    test::fuzz_check(open_reader(reader, stream, precision).has_value(), "a valid capture opens");

    test::fuzz_check(reader.snaplen() == static_cast<int>(snaplen), "snaplen comes from the file header");
    test::fuzz_check(reader.datalink() == static_cast<int>(linktype_ethernet), "datalink comes from the file header");
    test::fuzz_check(reader.major_version() == 2 && reader.minor_version() == 4,
                     "the version comes from the file header");
    test::fuzz_check(reader.is_swapped() == big_endian, "is_swapped tells a file of the other endianness");
    check_no_packet_yet(reader);

    // the default open reads with micro precision
    for (const Packet & packet : packets)
        check_packet(reader, packet, file_nano, precision == nano);

    const Result<bool> end = reader.read_next();
    test::fuzz_check(end.has_value() && !*end, "the end of the capture is not an error");
    test::fuzz_check(reader.close(), "the reader closes");
    test::fuzz_check(!reader.read_next().has_value(), "a closed reader refuses to read again");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_generated(provider);
    else
        fuzz_raw(provider);
    return 0;
}
