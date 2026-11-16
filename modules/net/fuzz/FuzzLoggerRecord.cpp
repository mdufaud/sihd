#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fuzz_helper.hpp>
#include <string>
#include <vector>

#include <sihd/net/LoggerRecord.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/LogInfo.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::net;
using namespace sihd::util;

namespace
{
void fuzz_decode(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = provider.ConsumeRemainingBytes<uint8_t>();
    LoggerRecord decoded;
    const auto done = decoded.decode(ArrByteView(bytes));
    if (!done)
    {
        test::fuzz_check(
            done.error().code == ErrorCode::invalid_argument || done.error().code == ErrorCode::not_supported,
            "a refused record is an invalid argument or another version");
        test::fuzz_check(decoded.message().empty(), "a refused record keeps the old one");
        return;
    }

    const ArrByte encoded = decoded.encode();
    test::fuzz_check(encoded.byte_size() == bytes.size() && std::memcmp(encoded.buf(), bytes.data(), bytes.size()) == 0,
                     "decoding then encoding gives the record back");
}

void fuzz_encode(FuzzedDataProvider & provider)
{
    const std::string source = provider.ConsumeRandomLengthString();
    const std::string thread_name = provider.ConsumeRandomLengthString();
    const std::string message = provider.ConsumeRemainingBytesAsString();
    LogInfo info(source, LogLevel::info);
    info.thread_name = thread_name;

    const LoggerRecord record(info, message);
    const ArrByte encoded = record.encode();
    test::fuzz_check(encoded.byte_size() == record.encoded_size(), "encode writes encoded_size bytes");

    LoggerRecord decoded;
    test::fuzz_check(decoded.decode(encoded).has_value(), "an encoded record decodes");
    test::fuzz_check(decoded.source() == source && decoded.thread_name() == thread_name && decoded.message() == message,
                     "the texts survive a round trip");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_encode(provider);
    else
        fuzz_decode(provider);
    return 0;
}
