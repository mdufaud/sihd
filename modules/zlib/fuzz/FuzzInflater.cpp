#include <cstddef>
#include <cstdint>
#include <string>

#include <sihd/util/Array.hpp>
#include <sihd/zlib/Deflater.hpp>
#include <sihd/zlib/Inflater.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;
using namespace sihd::zlib;

constexpr size_t max_output_size = 1 << 20;
const std::string known_message = "a message after the hostile input";

bool is_refusal(ErrorCode code)
{
    return code == ErrorCode::invalid_argument || code == ErrorCode::overflow;
}

std::string compress_known_message()
{
    Deflater deflater;
    Result<ArrByte> compressed = deflater.deflate_message(ArrByteView(known_message));
    test::fuzz_check(compressed.has_value(), "the known message compresses");
    return std::string(compressed->cpp_str_view());
}

void fuzz_hostile_input(FuzzedDataProvider & provider)
{
    const bool single_message = provider.ConsumeBool();
    const size_t limit = provider.ConsumeIntegralInRange<size_t>(0, max_output_size);
    const std::string hostile = provider.ConsumeRemainingBytesAsString();

    Inflater inflater(limit);
    Result<ArrByte> output = single_message ? inflater.inflate_message(ArrByteView(hostile))
                                            : inflater.inflate_stream(ArrByteView(hostile));
    if (output.has_value())
        test::fuzz_check(output->size() <= limit, "an output never exceeds the limit");
    else
        test::fuzz_check(is_refusal(output.error().code), "hostile input is refused as invalid or overflowing");

    if (single_message || output.has_value() == false)
    {
        Result<ArrByte> known = inflater.inflate_message(ArrByteView(compress_known_message()));
        if (limit >= known_message.size())
        {
            test::fuzz_check(known.has_value(), "a valid message decodes after a refused or ended one");
            test::fuzz_check(known->cpp_str_view() == known_message, "the valid message decodes intact");
        }
    }
}

void fuzz_round_trip(FuzzedDataProvider & provider)
{
    const Deflater::Format format = provider.ConsumeBool() ? Deflater::Format::gzip : Deflater::Format::zlib;
    const int level = provider.ConsumeIntegralInRange<int>(Deflater::min_level, Deflater::max_level);
    const bool message_mode = provider.ConsumeBool();
    const size_t chunks = provider.ConsumeIntegralInRange<size_t>(1, 8);

    Deflater deflater(format, level);
    Inflater inflater(max_output_size);
    for (size_t chunk = 0; chunk < chunks && provider.remaining_bytes() > 0; ++chunk)
    {
        const std::string content = provider.ConsumeRandomLengthString(max_output_size);
        const ArrByteView input(content);

        Result<ArrByte> compressed = message_mode ? deflater.deflate_message(input) : deflater.deflate_stream(input);
        test::fuzz_check(compressed.has_value(), "any content compresses");
        test::fuzz_check(compressed->size() <= deflater.output_bound(content.size()), "the output fits its bound");

        Result<ArrByte> inflated = message_mode ? inflater.inflate_message(*compressed)
                                                : inflater.inflate_stream(*compressed);
        test::fuzz_check(inflated.has_value(), "a compressed chunk decodes");
        test::fuzz_check(inflated->cpp_str_view() == content, "the content round trips");
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_hostile_input(provider);
    else
        fuzz_round_trip(provider);
    return 0;
}
