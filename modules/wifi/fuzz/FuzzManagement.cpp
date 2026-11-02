#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>

#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/management.hpp>

#include "wifi_fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;
using namespace sihd::wifi;
using test::Bytes;

namespace
{
constexpr size_t beacon_size = 12;
constexpr size_t assoc_request_size = 4;
constexpr size_t reassoc_request_size = 10;
constexpr size_t assoc_response_size = 6;
constexpr size_t authentication_size = 6;
constexpr size_t reason_size = 2;
constexpr size_t action_size = 1;

constexpr std::array<size_t, 7> boundary_sizes = {beacon_size,
                                                  assoc_request_size,
                                                  reassoc_request_size,
                                                  assoc_response_size,
                                                  authentication_size,
                                                  reason_size,
                                                  action_size};

uint16_t u16(const Bytes & body, size_t offset)
{
    return static_cast<uint16_t>(test::le_at(body, offset, 2));
}

void check_tail(ArrUByteView tail, const Bytes & body, size_t fixed_size)
{
    test::fuzz_check(test::is_view_of(tail, body, fixed_size, body.size() - fixed_size),
                     "the information elements are what follows the fixed fields");
}

template <typename T>
bool accepted(const Result<T> & result, const Bytes & body, size_t fixed_size)
{
    test::fuzz_check(result.has_value() == (body.size() >= fixed_size),
                     "a body is accepted exactly when it holds the fixed fields");
    if (!result.has_value())
        test::fuzz_check(result.error().code == ErrorCode::invalid_argument, "a short body is invalid_argument");
    return result.has_value();
}

void check_beacon(const Result<Beacon> & result, const Bytes & body)
{
    if (!accepted(result, body, beacon_size))
        return;
    test::fuzz_check(result->timestamp_us == test::le_at(body, 0, 8), "the timestamp is the first eight bytes");
    test::fuzz_check(result->interval_tu == u16(body, 8), "the interval follows the timestamp");
    test::fuzz_check(result->capability == u16(body, 10), "the capability follows the interval");
    test::fuzz_check(result->ess == ((result->capability & 0x0001) != 0), "ess is capability bit 0");
    test::fuzz_check(result->ibss == ((result->capability & 0x0002) != 0), "ibss is capability bit 1");
    test::fuzz_check(result->privacy == ((result->capability & 0x0010) != 0), "privacy is capability bit 4");
    check_tail(result->information_elements, body, beacon_size);
}

void check_assoc_request(const Result<AssocRequest> & result, const Bytes & body)
{
    if (!accepted(result, body, assoc_request_size))
        return;
    test::fuzz_check(result->capability == u16(body, 0), "the capability is the first field");
    test::fuzz_check(result->listen_interval == u16(body, 2), "the listen interval follows the capability");
    check_tail(result->information_elements, body, assoc_request_size);
}

void check_reassoc_request(const Result<ReassocRequest> & result, const Bytes & body)
{
    if (!accepted(result, body, reassoc_request_size))
        return;
    test::fuzz_check(result->capability == u16(body, 0), "the capability is the first field");
    test::fuzz_check(result->listen_interval == u16(body, 2), "the listen interval follows the capability");
    test::fuzz_check(
        Bytes(result->current_ap.begin(), result->current_ap.end()) == Bytes(body.begin() + 4, body.begin() + 10),
        "the current ap is the six bytes after the listen interval");
    check_tail(result->information_elements, body, reassoc_request_size);
}

void check_assoc_response(const Result<AssocResponse> & result, const Bytes & body)
{
    if (!accepted(result, body, assoc_response_size))
        return;
    test::fuzz_check(result->capability == u16(body, 0), "the capability is the first field");
    test::fuzz_check(result->status == u16(body, 2), "the status follows the capability");
    test::fuzz_check(result->aid == (u16(body, 4) & 0x3fff), "the aid is the low 14 bits of the last field");
    check_tail(result->information_elements, body, assoc_response_size);
}

void check_authentication(const Result<Authentication> & result, const Bytes & body)
{
    if (!accepted(result, body, authentication_size))
        return;
    test::fuzz_check(result->algorithm == u16(body, 0), "the algorithm is the first field");
    test::fuzz_check(result->sequence == u16(body, 2), "the sequence follows the algorithm");
    test::fuzz_check(result->status == u16(body, 4), "the status follows the sequence");
    check_tail(result->information_elements, body, authentication_size);
}

void check_reason(const Result<uint16_t> & result, const Bytes & body)
{
    if (accepted(result, body, reason_size))
        test::fuzz_check(*result == u16(body, 0), "the reason is two little endian bytes");
}

void check_action(const Result<Action> & result, const Bytes & body)
{
    if (!accepted(result, body, action_size))
        return;
    test::fuzz_check(result->category == body[0], "the category is the first byte");
    test::fuzz_check(test::is_view_of(result->body, body, action_size, body.size() - action_size),
                     "the action body follows the category");
}

void check_body(const Bytes & content)
{
    const Bytes body = test::exact_copy(content);
    const ArrUByteView view(body);
    check_beacon(parse_beacon(view), body);
    check_beacon(parse_probe_response(view), body);
    check_assoc_request(parse_assoc_request(view), body);
    check_reassoc_request(parse_reassoc_request(view), body);
    check_assoc_response(parse_assoc_response(view), body);
    check_authentication(parse_authentication(view), body);
    check_reason(parse_reason(view), body);
    check_action(parse_action(view), body);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
    {
        const size_t fixed = boundary_sizes[provider.ConsumeIntegralInRange<size_t>(0, boundary_sizes.size() - 1)];
        const size_t delta = provider.ConsumeIntegralInRange<size_t>(0, 3);
        check_body(test::consume_exact_bytes(provider, fixed - 1 + delta));
    }
    else
        check_body(provider.ConsumeRemainingBytes<uint8_t>());
    return 0;
}
