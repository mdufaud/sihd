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
        int days = 365;                             // validity from now
        uint64_t serial = 0;                        // 0 => random
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

        operator bool() const { return _handle != nullptr; }

        sihd::util::Result<void>
            generate_self_signed(const PrivateKey & key, std::string_view common_name, int days = 365);
        sihd::util::Result<void> generate_self_signed(const PrivateKey & key, const CertOptions & opts);

        sihd::util::Result<void> load_pem(std::string_view path);
        sihd::util::Result<void> save_pem(std::string_view path) const;
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

        void *native() const { return _handle.get(); }

    private:
        // type-erased X509: the deleter freeing it is bound at acquisition,
        // keeping openssl types out of this header
        std::shared_ptr<void> _handle;
};

} // namespace sihd::crypto

#endif
