#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/capabilities.hpp>
#include <sihd/wifi/ies.hpp>

using namespace sihd::wifi;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const sihd::util::ArrUByteView bytes(data, size);

    const std::vector<ies::Element> readable = ies::readable_elements(bytes);
    if (const auto strict = ies::elements(bytes); strict.has_value())
        test::fuzz_check(strict->size() == readable.size(), "a well formed buffer is fully readable");

    (void)ies::ssid(readable);
    (void)ies::channel(readable);
    (void)ies::rates(readable);
    (void)ies::vendors(readable);
    (void)ies::rsn(readable);
    (void)ies::wpa(readable);
    (void)ies::wps(readable);
    (void)ies::ht_capabilities(readable);
    (void)ies::vht_capabilities(readable);
    (void)ies::country(readable);
    (void)ies::tim(readable);

    (void)ies::parse_rsn(bytes);
    (void)ies::parse_wps(bytes);
    (void)ies::wps_attributes(bytes);
    return 0;
}
