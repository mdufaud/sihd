#include <cstddef>
#include <cstdint>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/management.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const sihd::util::ArrUByteView bytes(data, size);
    (void)sihd::wifi::parse_beacon(bytes);
    (void)sihd::wifi::parse_probe_response(bytes);
    (void)sihd::wifi::parse_assoc_request(bytes);
    (void)sihd::wifi::parse_reassoc_request(bytes);
    (void)sihd::wifi::parse_assoc_response(bytes);
    (void)sihd::wifi::parse_authentication(bytes);
    (void)sihd::wifi::parse_reason(bytes);
    (void)sihd::wifi::parse_action(bytes);
    return 0;
}
