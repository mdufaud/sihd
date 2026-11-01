#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <vector>

#include <sihd/crypto/digest.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/hash.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace digest = sihd::crypto::digest;
using namespace sihd::util;

namespace
{
std::vector<uint8_t> compute_standard(const char *algorithm, const std::vector<uint8_t> & input)
{
    const auto result = digest::compute(algorithm, input.data(), input.size());
    test::fuzz_check(result.has_value(), "a standard digest computes");
    return *result;
}

void fuzz_algorithm(FuzzedDataProvider & provider)
{
    const std::string algorithm = provider.ConsumeRandomLengthString(40);
    const std::vector<uint8_t> input = provider.ConsumeRemainingBytes<uint8_t>();
    (void)digest::compute(algorithm, input.data(), input.size());
}

void fuzz_standard(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> input = provider.ConsumeRemainingBytes<uint8_t>();

    const std::vector<uint8_t> sha1 = compute_standard("SHA-1", input);
    std::array<uint8_t, hash::sha1_size> reference {};
    hash::sha1(input.data(), input.size(), reference.data());
    test::fuzz_check(std::equal(sha1.begin(), sha1.end(), reference.begin(), reference.end()),
                     "sha-1 matches the independent implementation");

    const std::vector<uint8_t> sha256 = compute_standard("SHA-256", input);
    test::fuzz_check(sha256.size() == 32, "sha-256 is 32 bytes");
    const auto viewed = digest::compute("SHA-256", ArrByteView(input));
    test::fuzz_check(viewed.has_value() && *viewed == sha256, "the view overload computes the same digest");

    test::fuzz_check(compute_standard("SHA-512", input).size() == 64, "sha-512 is 64 bytes");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_standard(provider);
    else
        fuzz_algorithm(provider);
    return 0;
}
