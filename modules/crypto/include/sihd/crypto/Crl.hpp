#ifndef __SIHD_CRYPTO_CRL_HPP__
#define __SIHD_CRYPTO_CRL_HPP__

#include <cstddef>
#include <cstdint>
#include <memory>

#include <sihd/crypto/RevocationInfo.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::crypto
{

class Certificate;

class Crl
{
    public:
        Crl() = default;
        ~Crl() = default;

        Crl(const Crl & other) = default;
        Crl & operator=(const Crl & other) = default;
        Crl(Crl && other) noexcept = default;
        Crl & operator=(Crl && other) noexcept = default;

        operator bool() const;

        sihd::util::Result<void> load_der(const uint8_t *data, size_t len);

        sihd::util::Result<void> verify(const Certificate & issuer) const;
        // unknown when cert has another issuer or lies outside the CRL partition
        sihd::util::Result<RevocationInfo> status(const Certificate & cert) const;

        sihd::util::Timestamp this_update() const;
        // 0 when the CRL gives none
        sihd::util::Timestamp next_update() const;

        void clear();

    private:
        // type-erased X509_CRL, deleter bound at acquisition: keeps openssl types out of this header
        std::shared_ptr<void> _handle;
};

} // namespace sihd::crypto

#endif
