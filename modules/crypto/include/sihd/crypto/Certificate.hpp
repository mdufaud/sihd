#ifndef __SIHD_CRYPTO_CERTIFICATE_HPP__
#define __SIHD_CRYPTO_CERTIFICATE_HPP__

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
        Certificate();
        ~Certificate();

        Certificate(const Certificate & other);
        Certificate & operator=(const Certificate & other);
        Certificate(Certificate && other) noexcept;
        Certificate & operator=(Certificate && other) noexcept;

        operator bool() const { return _handle != nullptr; }

        bool generate_self_signed(const PrivateKey & key, std::string_view common_name, int days = 365);
        bool generate_self_signed(const PrivateKey & key, const CertOptions & opts);

        bool load_pem(std::string_view path);
        bool save_pem(std::string_view path) const;
        bool load_der(const uint8_t *data, size_t len);

        bool load_pem_string(std::string_view pem);
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

        void *native() const { return _handle; }

    private:
        void *_handle;
};

} // namespace sihd::crypto

#endif
