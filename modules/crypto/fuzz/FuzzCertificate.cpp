#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <sihd/crypto/Certificate.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::crypto;

namespace
{
auto describe(const Certificate & certificate)
{
    (void)certificate.not_before();
    (void)certificate.not_after();
    return std::make_tuple(certificate.subject_common_name(),
                           certificate.issuer_common_name(),
                           certificate.subject_alt_names(),
                           certificate.serial_hex(),
                           certificate.is_ca());
}

void check_loaded(const Certificate & certificate)
{
    test::fuzz_check(static_cast<bool>(certificate), "a loaded certificate is set");
    const auto description = describe(certificate);

    const std::string pem = certificate.to_pem_string();
    if (pem.empty())
        return;
    Certificate reloaded;
    test::fuzz_check(reloaded.load_pem_string(pem).has_value(), "the pem of a certificate loads again");
    test::fuzz_check(reloaded.to_pem_string() == pem, "the pem of a certificate round trips");
    test::fuzz_check(describe(reloaded) == description, "a reloaded certificate describes the same");
}

void fuzz_der(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> der = provider.ConsumeRemainingBytes<uint8_t>();
    if (der.empty())
        return;
    Certificate certificate;
    if (certificate.load_der(der.data(), der.size()))
        check_loaded(certificate);
}

void fuzz_pem(FuzzedDataProvider & provider)
{
    const std::vector<char> pem = provider.ConsumeRemainingBytes<char>();
    if (pem.empty())
        return;
    Certificate certificate;
    if (certificate.load_pem_string(std::string_view(pem.data(), pem.size())))
        check_loaded(certificate);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_pem(provider);
    else
        fuzz_der(provider);
    return 0;
}
