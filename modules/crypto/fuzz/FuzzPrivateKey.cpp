#include <array>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/crypto/PrivateKey.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::crypto;

namespace
{
constexpr std::array<std::string_view, 3> curves = {"prime256v1", "secp384r1", "secp521r1"};
constexpr std::string_view pem_header = "-----BEGIN PRIVATE KEY-----\n";
constexpr std::string_view pem_footer = "-----END PRIVATE KEY-----\n";

void fuzz_loaded(FuzzedDataProvider & provider)
{
    const std::vector<char> pem = provider.ConsumeRemainingBytes<char>();
    if (pem.empty())
        return;

    PrivateKey key;
    if (!key.load_pem_string(std::string_view(pem.data(), pem.size())))
        return;
    test::fuzz_check(static_cast<bool>(key), "a loaded key is set");

    const std::string encoded = key.to_pem_string();
    if (encoded.empty())
        return;
    PrivateKey reloaded;
    test::fuzz_check(reloaded.load_pem_string(encoded).has_value(), "the pem of a key loads again");
    test::fuzz_check(reloaded.to_pem_string() == encoded, "the pem of a key round trips");
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    const std::string_view curve = provider.PickValueInArray(curves);
    PrivateKey key;
    test::fuzz_check(!key && key.to_pem_string().empty(), "a key without material has no pem");
    test::fuzz_check(key.generate_ec(curve).has_value() && static_cast<bool>(key), "a known curve generates a key");

    const std::string encoded = key.to_pem_string();
    test::fuzz_check(encoded.starts_with(pem_header) && encoded.ends_with(pem_footer),
                     "a generated key is a pkcs8 pem");

    PrivateKey reloaded;
    test::fuzz_check(reloaded.load_pem_string(encoded).has_value(), "the pem of a generated key loads");
    test::fuzz_check(reloaded.to_pem_string() == encoded, "the pem of a generated key round trips");

    PrivateKey other;
    test::fuzz_check(other.generate_ec(curve).has_value() && other.to_pem_string() != encoded,
                     "two generated keys differ");

    const std::string unknown = "zz" + test::consume_word(provider, 1, 8);
    test::fuzz_check(!key.generate_ec(unknown).has_value(), "an unknown curve does not generate a key");
    test::fuzz_check(static_cast<bool>(key) && key.to_pem_string() == encoded,
                     "a failed generation keeps the previous key");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_generated(provider);
    else
        fuzz_loaded(provider);
    return 0;
}
