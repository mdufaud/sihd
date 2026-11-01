#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/pcap/PcapReader.hpp>
#include <sihd/util/ArrayView.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::pcap;
using namespace sihd::util;

namespace
{
Result<void> open_reader(PcapReader & reader, FILE *stream, int precision)
{
    switch (precision)
    {
        case 1:
            return reader.open_micro_precision(stream);
        case 2:
            return reader.open_nano_precision(stream);
        default:
            return reader.open(stream);
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const int precision = provider.ConsumeIntegralInRange<int>(0, 2);
    std::vector<char> bytes = provider.ConsumeRemainingBytes<char>();
    if (bytes.empty())
        return 0;

    FILE *stream = fmemopen(bytes.data(), bytes.size(), "r");
    if (stream == nullptr)
        return 0;

    PcapReader reader("pcap-fuzz");
    if (!open_reader(reader, stream, precision).has_value())
    {
        // a refused stream stays open, an accepted one is closed with the reader
        fclose(stream);
        return 0;
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
            test::fuzz_check(view.size() <= reader.packet_caplen(), "the packet data stays within the captured bytes");
        (void)reader.get_read_timestamp();
    }
    return 0;
}
