#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/data.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{
constexpr size_t llc_snap_size = 8;
constexpr uint8_t snap_header[] = {0xaa, 0xaa, 0x03};

void check_llc(const std::vector<uint8_t> & body)
{
    const Result<Llc> llc = parse_llc_snap(ArrUByteView(body));
    if (body.size() < llc_snap_size)
    {
        test::fuzz_check(!llc.has_value(), "a body shorter than the llc snap header is rejected");
        return;
    }

    const bool snap = std::equal(std::begin(snap_header), std::end(snap_header), body.begin());
    test::fuzz_check(llc.has_value() == snap, "a snap header is accepted and any other one is not");
    if (!llc.has_value())
    {
        test::fuzz_check(llc.error().code == ErrorCode::not_supported, "another header is not_supported");
        return;
    }
    test::fuzz_check(llc->oui == Oui({body[3], body[4], body[5]}), "the oui follows the snap header");
    test::fuzz_check(llc->ethertype == ((body[6] << 8) | body[7]), "the ethertype is big endian");
    test::fuzz_check(llc->payload.data() == body.data() + llc_snap_size, "the payload is a view of the body");
    test::fuzz_check(llc->payload.size() == body.size() - llc_snap_size, "the payload is what follows the header");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const bool force_snap = provider.ConsumeBool();
    std::vector<uint8_t> body = provider.ConsumeRemainingBytes<uint8_t>();
    if (force_snap && body.size() >= std::size(snap_header))
        std::copy(std::begin(snap_header), std::end(snap_header), body.begin());
    check_llc(body);
    return 0;
}
