#ifndef __SIHD_CRYPTO_CERTIFICATE_HPP__
#define __SIHD_CRYPTO_CERTIFICATE_HPP__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::crypto
{

class PrivateKey;

struct CertOptions
{
        std::string common_name;
        std::string organization;
        std::string organizational_unit;
        std::string country; // 2-letter ISO
        std::string state;
        std::string locality;
        std::string email;
        std::vector<std::string> subject_alt_names; // "DNS:host" / "IP:1.2.3.4"
        int days = 365;
        uint64_t serial = 0; // 0 => random
        bool is_ca = false;
        // opt out of the DNS/IP/email/URI kinds: otherName, RID, dirName pass, separators never do
        bool any_alt_name_kind = false;
};

class Certificate
{
    public:
        Certificate() = default;
        ~Certificate() = default;

        Certificate(const Certificate & other) = default;
        Certificate & operator=(const Certificate & other) = default;
        Certificate(Certificate && other) noexcept = default;
        Certificate & operator=(Certificate && other) noexcept = default;

        operator bool() const;

        sihd::util::Result<void>
            generate_self_signed(const PrivateKey & key, const std::string & common_name, int days = 365);
        sihd::util::Result<void> generate_self_signed(const PrivateKey & key, const CertOptions & opts);

        sihd::util::Result<void> load_pem(sihd::util::ZStringView path);
        sihd::util::Result<void> save_pem(const std::string & path) const;
        sihd::util::Result<void> load_der(const uint8_t *data, size_t len);

        sihd::util::Result<void> load_pem_string(std::string_view pem);
        std::string to_pem_string() const;

        bool verify(const Certificate & ca) const;

        std::string subject_common_name() const;
        std::string issuer_common_name() const;
        std::vector<std::string> subject_alt_names() const;
        sihd::util::Timestamp not_before() const;
        sihd::util::Timestamp not_after() const;
        std::string serial_hex() const;
        bool is_ca() const;

        void clear();

        void *native() const;

    private:
        // type-erased X509, deleter bound at acquisition: keeps openssl types out of this header
        std::shared_ptr<void> _handle;
};

} // namespace sihd::crypto

#endif
