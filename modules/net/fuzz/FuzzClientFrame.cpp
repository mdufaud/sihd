#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/net/ClientFrame.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::net;
using namespace sihd::util;

namespace
{
void check_frame(const ClientFrame & frame, uint64_t client_id, std::string_view payload)
{
    test::fuzz_check(frame.client_id() == client_id, "the client id is kept");
    test::fuzz_check(std::string_view(frame.payload()) == payload, "the payload is kept");

    const ArrByte encoded = frame.encode();
    test::fuzz_check(encoded.byte_size() == ClientFrame::header_size + payload.size(),
                     "an encoded frame is its header then its payload");
    uint64_t encoded_id = 0;
    std::memcpy(&encoded_id, encoded.buf(), sizeof(encoded_id));
    test::fuzz_check(encoded_id == client_id, "the encoded header is the client id in host byte order");
    test::fuzz_check(
        payload.empty() || std::memcmp(encoded.buf() + ClientFrame::header_size, payload.data(), payload.size()) == 0,
        "the encoded payload follows the header");

    const auto decoded = ClientFrame::decode(encoded);
    test::fuzz_check(decoded.has_value(), "an encoded frame decodes");
    test::fuzz_check(decoded->client_id() == client_id, "the client id survives a round trip");
    test::fuzz_check(std::string_view(decoded->payload()) == payload, "the payload survives a round trip");
}

// a view without storage: encoding must not copy from its null pointer
void check_empty_frame(uint64_t client_id)
{
    const ClientFrame frame(client_id, ArrCharView());
    test::fuzz_check(frame.payload().byte_size() == 0, "a frame without payload has an empty payload");

    const ArrByte encoded = frame.encode();
    test::fuzz_check(encoded.byte_size() == ClientFrame::header_size, "a frame without payload is only its header");
    uint64_t encoded_id = 0;
    std::memcpy(&encoded_id, encoded.buf(), sizeof(encoded_id));
    test::fuzz_check(encoded_id == client_id, "a frame without payload keeps its client id");
}

void fuzz_decode(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> bytes = provider.ConsumeRemainingBytes<uint8_t>();
    const auto decoded = ClientFrame::decode(ArrByteView(bytes));
    test::fuzz_check(decoded.has_value() == (bytes.size() >= ClientFrame::header_size),
                     "a frame decodes exactly when it holds the header");
    if (!decoded)
    {
        test::fuzz_check(decoded.error().code == ErrorCode::invalid_argument, "a short frame is an invalid argument");
        return;
    }

    uint64_t client_id = 0;
    std::memcpy(&client_id, bytes.data(), sizeof(client_id));
    const std::string_view payload(reinterpret_cast<const char *>(bytes.data()) + ClientFrame::header_size,
                                   bytes.size() - ClientFrame::header_size);
    test::fuzz_check(decoded->payload().buf() == bytes.data() + ClientFrame::header_size,
                     "the decoded payload points into the frame");
    check_frame(*decoded, client_id, payload);

    const ArrByte encoded = decoded->encode();
    test::fuzz_check(encoded.byte_size() == bytes.size() && std::memcmp(encoded.buf(), bytes.data(), bytes.size()) == 0,
                     "decoding then encoding gives the frame back");
}

void fuzz_encode(FuzzedDataProvider & provider)
{
    const uint64_t client_id = provider.ConsumeIntegral<uint64_t>();
    const std::string payload = provider.ConsumeRemainingBytesAsString();
    check_frame(ClientFrame(client_id, ArrCharView(payload.data(), payload.size())), client_id, payload);
    check_empty_frame(client_id);
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
