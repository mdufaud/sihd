#ifndef __SIHD_CRYPTO_OCSPREQUEST_HPP__
#define __SIHD_CRYPTO_OCSPREQUEST_HPP__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <sihd/crypto/RevocationInfo.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::crypto
{

class Certificate;

class OcspRequest
{
    public:
        OcspRequest() = default;
        ~OcspRequest() = default;

        OcspRequest(const OcspRequest & other) = delete;
        OcspRequest & operator=(const OcspRequest & other) = delete;
        OcspRequest(OcspRequest && other) noexcept = default;
        OcspRequest & operator=(OcspRequest && other) noexcept = default;

        operator bool() const;

        sihd::util::Result<void> build(const Certificate & cert, const Certificate & issuer);
        sihd::util::Result<std::vector<uint8_t>> to_der() const;

        // chain ends with the trust anchor; a response without nonce is accepted, a different one is not
        sihd::util::Result<RevocationInfo>
            verify_response(const uint8_t *data, size_t len, const std::vector<Certificate> & chain) const;

        void clear();

    private:
        struct RequestFree
        {
                void operator()(void *request) const;
        };

        // type-erased OCSP_REQUEST, freed in the source: keeps openssl types out of this header
        std::unique_ptr<void, RequestFree> _handle;
};

} // namespace sihd::crypto

#endif
