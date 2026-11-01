#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/crypto/PrivateKey.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::crypto;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const std::vector<char> pem = provider.ConsumeRemainingBytes<char>();
    if (pem.empty())
        return 0;

    PrivateKey key;
    if (!key.load_pem_string(std::string_view(pem.data(), pem.size())))
        return 0;
    test::fuzz_check(static_cast<bool>(key), "a loaded key is set");

    const std::string encoded = key.to_pem_string();
    if (encoded.empty())
        return 0;
    PrivateKey reloaded;
    test::fuzz_check(reloaded.load_pem_string(encoded).has_value(), "the pem of a key loads again");
    test::fuzz_check(reloaded.to_pem_string() == encoded, "the pem of a key round trips");
    return 0;
}
