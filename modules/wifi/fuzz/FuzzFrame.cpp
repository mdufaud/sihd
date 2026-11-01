#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/frame.hpp>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    (void)sihd::wifi::parse_mac(provider.ConsumeRandomLengthString(24));
    const std::vector<uint8_t> frame = provider.ConsumeRemainingBytes<uint8_t>();
    const sihd::util::ArrUByteView bytes(frame.data(), frame.size());
    (void)sihd::wifi::parse_frame(bytes);
    (void)sihd::wifi::dissect_80211(bytes);
    return 0;
}
