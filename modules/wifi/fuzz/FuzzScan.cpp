#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/scan.hpp>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{
// microsoft oui, wpa type
constexpr std::array<uint8_t, 4> wpa_header = {0x00, 0x50, 0xf2, 0x01};

bool announces_rsn_or_wpa(const std::vector<ies::Element> & elements)
{
    for (const ies::Element & element : elements)
    {
        if (element.id == ies::Id::rsn)
            return true;
        if (element.id == ies::Id::vendor && element.data.size() >= wpa_header.size()
            && std::equal(wpa_header.begin(), wpa_header.end(), element.data.begin()))
            return true;
    }
    return false;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const std::vector<uint8_t> elements(data, data + size);
    const ArrUByteView view(elements);

    const Security without_privacy = security_of(view, false);
    const Security with_privacy = security_of(view, true);

    const std::vector<uint8_t> copy(elements.begin(), elements.end());
    test::fuzz_check(security_of(ArrUByteView(copy), false) == without_privacy, "security is deterministic");
    if (without_privacy == Security::open)
        test::fuzz_check(with_privacy == Security::wep, "the privacy bit turns open into wep");
    else
        test::fuzz_check(with_privacy == without_privacy, "the privacy bit only matters to an open network");

    const Result<std::vector<ies::Element>> parsed = ies::elements(view);
    if (!parsed.has_value())
    {
        test::fuzz_check(without_privacy == Security::unknown, "a damaged element leaves security unknown");
        return 0;
    }
    if (!announces_rsn_or_wpa(*parsed))
    {
        test::fuzz_check(without_privacy == Security::open, "no rsn nor wpa and no privacy is open");
        test::fuzz_check(with_privacy == Security::wep, "no rsn nor wpa but privacy is wep");
    }
    return 0;
}
