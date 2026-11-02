#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/crypto/cipher.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
namespace cipher = sihd::crypto::cipher;

constexpr const char *algorithm = "AES-256-CBC";

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::vector<uint8_t> key = provider.ConsumeBytes<uint8_t>(32);
    std::vector<uint8_t> iv = provider.ConsumeBytes<uint8_t>(16);
    key.resize(32);
    iv.resize(16);
    const std::vector<uint8_t> plaintext = provider.ConsumeBytes<uint8_t>(
        provider.ConsumeIntegralInRange<size_t>(0, 512));
    const std::vector<uint8_t> garbage = provider.ConsumeRemainingBytes<uint8_t>();

    const auto encrypted = cipher::encrypt(algorithm,
                                           key.data(),
                                           key.size(),
                                           iv.data(),
                                           iv.size(),
                                           plaintext.data(),
                                           plaintext.size());
    test::fuzz_check(encrypted.has_value(), "a plaintext encrypts");
    test::fuzz_check(encrypted->size() % 16 == 0 && encrypted->size() > plaintext.size(), "cbc pads to a whole block");

    const auto decrypted = cipher::decrypt(algorithm,
                                           key.data(),
                                           key.size(),
                                           iv.data(),
                                           iv.size(),
                                           encrypted->data(),
                                           encrypted->size());
    test::fuzz_check(decrypted.has_value(), "a ciphertext decrypts with the same key");
    test::fuzz_check(decrypted->size() == plaintext.size(), "decryption keeps the plaintext size");
    test::fuzz_check(std::equal(plaintext.begin(), plaintext.end(), decrypted->data()),
                     "decryption restores the plaintext");

    (void)cipher::decrypt(algorithm, key.data(), key.size(), iv.data(), iv.size(), garbage.data(), garbage.size());

    test::fuzz_check(!cipher::encrypt("NULL", key.data(), 0, iv.data(), 0, plaintext.data(), plaintext.size()),
                     "a cipher without a key does not encrypt");
    test::fuzz_check(!cipher::encrypt(algorithm, key.data(), 31, iv.data(), 16, plaintext.data(), plaintext.size()),
                     "a key of the wrong size is refused");
    test::fuzz_check(!cipher::encrypt(algorithm, key.data(), 32, iv.data(), 15, plaintext.data(), plaintext.size()),
                     "an iv of the wrong size is refused");
    test::fuzz_check(
        cipher::encrypt("AES-128-ECB", key.data(), 16, iv.data(), 16, plaintext.data(), plaintext.size()).has_value(),
        "a mode without iv ignores the iv");
    return 0;
}
