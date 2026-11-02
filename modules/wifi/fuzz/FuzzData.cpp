#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/data.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{
constexpr size_t llc_snap_size = 8;
constexpr uint8_t snap_header[] = {0xaa, 0xaa, 0x03};

void check_llc(const std::vector<uint8_t> & content)
{
    const std::vector<uint8_t> body = test::exact_copy(content);
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

// 0 intact, 1 to 3 one byte of the snap header wrong, 4 cut short of the header
void check_built(FuzzedDataProvider & provider)
{
    const Oui oui = {provider.ConsumeIntegral<uint8_t>(),
                     provider.ConsumeIntegral<uint8_t>(),
                     provider.ConsumeIntegral<uint8_t>()};
    const uint16_t ethertype = provider.ConsumeIntegral<uint16_t>();
    const std::vector<uint8_t> payload = provider.ConsumeBytes<uint8_t>(provider.ConsumeIntegralInRange<size_t>(0, 8));
    const int damage = provider.ConsumeIntegralInRange<int>(0, 4);

    std::vector<uint8_t> body(std::begin(snap_header), std::end(snap_header));
    body.insert(body.end(), oui.begin(), oui.end());
    test::put_be(body, ethertype, 2);
    test::put_bytes(body, payload);
    if (damage >= 1 && damage <= 3)
        body[damage - 1] ^= provider.ConsumeIntegralInRange<uint8_t>(1, 255);
    if (damage == 4)
        body.resize(provider.ConsumeIntegralInRange<size_t>(0, llc_snap_size - 1));

    const std::vector<uint8_t> input = test::exact_copy(body);
    const Result<Llc> llc = parse_llc_snap(ArrUByteView(input));
    test::fuzz_check(llc.has_value() == (damage == 0), "only a whole snap header is accepted");
    if (damage != 0)
    {
        const ErrorCode code = damage == 4 ? ErrorCode::invalid_argument : ErrorCode::not_supported;
        test::fuzz_check(llc.error().code == code, "a short body is invalid_argument and another header not_supported");
        return;
    }
    test::fuzz_check(llc->oui == oui, "the oui is what follows the snap header");
    test::fuzz_check(llc->ethertype == ethertype, "the ethertype is big endian");
    test::fuzz_check(test::is_view_of(llc->payload, input, llc_snap_size, payload.size()),
                     "the payload is what follows the header, even when empty");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const int mode = provider.ConsumeIntegralInRange<int>(0, 2);
    if (mode == 2)
    {
        check_built(provider);
        return 0;
    }
    std::vector<uint8_t> body = provider.ConsumeRemainingBytes<uint8_t>();
    if (mode == 1 && body.size() >= std::size(snap_header))
        std::copy(std::begin(snap_header), std::end(snap_header), body.begin());
    check_llc(body);
    return 0;
}
