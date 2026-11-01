#include <cstddef>
#include <cstdint>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/radiotap.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const sihd::util::ArrUByteView bytes(data, size);
    (void)sihd::wifi::parse_radiotap(bytes);
    (void)sihd::wifi::dissect_radiotap(bytes);
    return 0;
}
