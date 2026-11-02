#include <arpa/inet.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fuzz_helper.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fmt/format.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>

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

void check_loaded(const Certificate & certificate, bool pem_required = false)
{
    test::fuzz_check(static_cast<bool>(certificate), "a loaded certificate is set");
    const auto description = describe(certificate);

    const std::string pem = certificate.to_pem_string();
    test::fuzz_check(!pem_required || pem.starts_with("-----BEGIN CERTIFICATE-----\n"),
                     "a generated certificate has a pem");
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

PrivateKey make_key()
{
    PrivateKey key;
    test::fuzz_check(key.generate_ec().has_value(), "an ec key generates");
    return key;
}

const PrivateKey & signing_key(size_t index)
{
    static const std::array<PrivateKey, 2> keys = {make_key(), make_key()};
    return keys[index];
}

std::string even_upper_hex(uint64_t value)
{
    std::string hex = fmt::format("{:X}", value);
    if (hex.size() % 2 == 1)
        hex.insert(hex.begin(), '0');
    return hex;
}

std::string consume_alt_name(FuzzedDataProvider & provider)
{
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            return "DNS:" + test::consume_text(provider, "abcdefghijklmnopqrstuvwxyz0123456789.-", 1, 12);
        case 1:
        {
            const std::vector<uint8_t> octets = test::consume_exact_bytes(provider, 4);
            return fmt::format("IP:{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3]);
        }
        default:
        {
            const std::vector<uint8_t> octets = test::consume_exact_bytes(provider, 16);
            std::array<char, INET6_ADDRSTRLEN> text {};
            test::fuzz_check(inet_ntop(AF_INET6, octets.data(), text.data(), text.size()) != nullptr, "an ipv6 prints");
            return std::string("IP:") + text.data();
        }
    }
}

void fuzz_generated(FuzzedDataProvider & provider)
{
    CertOptions options;
    options.common_name = test::consume_word(provider, 0, 16);
    options.days = provider.ConsumeIntegralInRange<int>(1, 800);
    options.serial = provider.ConsumeBool() ? 0 : provider.ConsumeIntegral<uint64_t>();
    options.is_ca = provider.ConsumeBool();
    for (size_t i = provider.ConsumeIntegralInRange<size_t>(0, 3); i > 0; --i)
        options.subject_alt_names.push_back(consume_alt_name(provider));

    const std::time_t before = std::time(nullptr);
    Certificate certificate;
    test::fuzz_check(certificate.generate_self_signed(signing_key(0), options).has_value(),
                     "a certificate generates from valid options");
    const std::time_t after = std::time(nullptr);

    test::fuzz_check(certificate.subject_common_name() == options.common_name, "the subject common name is kept");
    test::fuzz_check(certificate.issuer_common_name() == options.common_name,
                     "a self signed certificate issues itself");
    test::fuzz_check(certificate.subject_alt_names() == options.subject_alt_names, "the alt names are kept in order");
    test::fuzz_check(certificate.is_ca() == options.is_ca, "the ca flag is kept");
    if (options.serial != 0)
        test::fuzz_check(certificate.serial_hex() == even_upper_hex(options.serial), "the serial is kept");
    else
        test::fuzz_check(!certificate.serial_hex().empty() && certificate.serial_hex().size() % 2 == 0,
                         "a random serial is a hex string of whole bytes");

    const int64_t not_before = certificate.not_before().seconds();
    const int64_t not_after = certificate.not_after().seconds();
    test::fuzz_check(not_before >= before && not_before <= after, "a certificate is valid from its creation");
    const int64_t validity = not_after - not_before;
    test::fuzz_check(validity >= options.days * 86400LL - 1 && validity <= options.days * 86400LL + 1,
                     "a certificate is valid for the requested days");

    test::fuzz_check(certificate.verify(certificate), "a self signed certificate verifies against itself");
    Certificate stranger;
    test::fuzz_check(stranger.generate_self_signed(signing_key(1), options).has_value(),
                     "a certificate generates with another key");
    test::fuzz_check(!certificate.verify(stranger), "a certificate does not verify against another key");

    check_loaded(certificate, true);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_der(provider);
            break;
        case 1:
            fuzz_pem(provider);
            break;
        default:
            fuzz_generated(provider);
            break;
    }
    return 0;
}
