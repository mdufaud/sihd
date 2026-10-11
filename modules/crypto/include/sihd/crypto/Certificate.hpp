#ifndef __SIHD_CRYPTO_CERTIFICATE_HPP__
#define __SIHD_CRYPTO_CERTIFICATE_HPP__

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>
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

        std::expected<void, sihd::util::Error>
            generate_self_signed(const PrivateKey & key, std::string_view common_name, int days = 365);
        std::expected<void, sihd::util::Error> generate_self_signed(const PrivateKey & key, const CertOptions & opts);

        std::expected<void, sihd::util::Error> load_pem(std::string_view path);
        std::expected<void, sihd::util::Error> save_pem(std::string_view path) const;
        std::expected<void, sihd::util::Error> load_der(const uint8_t *data, size_t len);

        std::expected<void, sihd::util::Error> load_pem_string(std::string_view pem);
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
