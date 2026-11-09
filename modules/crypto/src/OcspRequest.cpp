#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/OcspRequest.hpp>
#include <sihd/crypto/error.hpp>

#include "asn1_time.hpp"
#include <openssl/err.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

struct OcspRequestFree
{
        void operator()(OCSP_REQUEST *request) const { OCSP_REQUEST_free(request); }
};
struct OcspResponseFree
{
        void operator()(OCSP_RESPONSE *response) const { OCSP_RESPONSE_free(response); }
};
struct OcspBasicFree
{
        void operator()(OCSP_BASICRESP *basic) const { OCSP_BASICRESP_free(basic); }
};
struct X509StoreFree
{
        void operator()(X509_STORE *store) const { X509_STORE_free(store); }
};
// the certificates stay owned by their Certificate
struct X509StackFree
{
        void operator()(STACK_OF(X509) * stack) const { sk_X509_free(stack); }
};

RevocationStatus status_from_ocsp(int status)
{
    switch (status)
    {
        case V_OCSP_CERTSTATUS_GOOD:
            return RevocationStatus::good;
        case V_OCSP_CERTSTATUS_REVOKED:
            return RevocationStatus::revoked;
        default:
            return RevocationStatus::unknown;
    }
}

} // namespace

void OcspRequest::RequestFree::operator()(void *request) const
{
    OCSP_REQUEST_free(static_cast<OCSP_REQUEST *>(request));
}

Result<void> OcspRequest::build(const Certificate & cert, const Certificate & issuer)
{
    ERR_clear_error();
    if (!cert || !issuer)
        return std::unexpected(Error(not_initialized, "cannot build an ocsp request without certificate and issuer"));
    OCSP_CERTID *id = OCSP_cert_to_id(nullptr,
                                      static_cast<X509 *>(cert.native()),
                                      static_cast<X509 *>(issuer.native()));
    if (id == nullptr)
        return make_error("OCSP_cert_to_id");
    std::unique_ptr<OCSP_REQUEST, OcspRequestFree> request(OCSP_REQUEST_new());
    if (request == nullptr || OCSP_request_add0_id(request.get(), id) == nullptr)
    {
        OCSP_CERTID_free(id);
        return make_error("OCSP_request_add0_id");
    }
    if (OCSP_request_add1_nonce(request.get(), nullptr, -1) != 1)
        return make_error("OCSP_request_add1_nonce");
    _handle.reset(request.release());
    return {};
}

Result<std::vector<uint8_t>> OcspRequest::to_der() const
{
    ERR_clear_error();
    if (!_handle)
        return std::unexpected(Error(not_initialized, "no ocsp request built"));
    auto *request = static_cast<OCSP_REQUEST *>(_handle.get());
    const int len = i2d_OCSP_REQUEST(request, nullptr);
    if (len <= 0)
        return make_error("i2d_OCSP_REQUEST");
    std::vector<uint8_t> der(static_cast<size_t>(len));
    unsigned char *out = der.data();
    if (i2d_OCSP_REQUEST(request, &out) != len)
        return make_error("i2d_OCSP_REQUEST");
    return der;
}

Result<RevocationInfo>
    OcspRequest::verify_response(const uint8_t *data, size_t len, const std::vector<Certificate> & chain) const
{
    ERR_clear_error();
    if (!_handle)
        return std::unexpected(Error(not_initialized, "no ocsp request built"));
    if (chain.empty() || !chain.back())
        return std::unexpected(Error(invalid_argument, "ocsp verification needs a trust anchor"));

    const unsigned char *in = data;
    std::unique_ptr<OCSP_RESPONSE, OcspResponseFree> response(d2i_OCSP_RESPONSE(nullptr, &in, static_cast<long>(len)));
    if (response == nullptr)
        return make_error("read ocsp response");
    const int response_status = OCSP_response_status(response.get());
    if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL)
        return std::unexpected(
            Error(io_error, "ocsp responder answered {}", OCSP_response_status_str(response_status)));
    std::unique_ptr<OCSP_BASICRESP, OcspBasicFree> basic(OCSP_response_get1_basic(response.get()));
    if (basic == nullptr)
        return make_error("OCSP_response_get1_basic");

    auto *request = static_cast<OCSP_REQUEST *>(_handle.get());
    if (OCSP_check_nonce(request, basic.get()) == 0)
    {
        ERR_clear_error();
        return std::unexpected(Error(invalid_argument, "ocsp response nonce does not match the request"));
    }

    std::unique_ptr<X509_STORE, X509StoreFree> store(X509_STORE_new());
    std::unique_ptr<STACK_OF(X509), X509StackFree> certs(sk_X509_new_null());
    if (store == nullptr || certs == nullptr
        || X509_STORE_add_cert(store.get(), static_cast<X509 *>(chain.back().native())) != 1)
        return make_error("ocsp trust store");
    for (const Certificate & cert : chain)
    {
        if (cert && sk_X509_push(certs.get(), static_cast<X509 *>(cert.native())) <= 0)
            return make_error("ocsp certificate chain");
    }
    if (OCSP_basic_verify(basic.get(), certs.get(), store.get(), 0) != 1)
    {
        const char *reason_text = ERR_reason_error_string(ERR_peek_last_error());
        ERR_clear_error();
        return std::unexpected(Error(invalid_argument,
                                     "ocsp response is not signed by a trusted responder: {}",
                                     reason_text != nullptr ? reason_text : "unknown reason"));
    }

    OCSP_CERTID *id = OCSP_onereq_get0_id(OCSP_request_onereq_get0(request, 0));
    int cert_status = V_OCSP_CERTSTATUS_UNKNOWN;
    int reason = 0;
    ASN1_GENERALIZEDTIME *revocation_time = nullptr;
    ASN1_GENERALIZEDTIME *this_update = nullptr;
    ASN1_GENERALIZEDTIME *next_update = nullptr;
    if (OCSP_resp_find_status(basic.get(), id, &cert_status, &reason, &revocation_time, &this_update, &next_update)
        != 1)
        return std::unexpected(Error(not_found, "ocsp response has no status for the certificate"));
    if (OCSP_check_validity(this_update, next_update, validity_leeway_seconds, -1) != 1)
    {
        ERR_clear_error();
        return std::unexpected(Error(invalid_argument, "ocsp response is outside its validity period"));
    }

    RevocationInfo info;
    info.status = status_from_ocsp(cert_status);
    info.source = RevocationSource::ocsp;
    info.this_update = asn1_time_to_timestamp(this_update);
    info.next_update = asn1_time_to_timestamp(next_update);
    info.revocation_time = asn1_time_to_timestamp(revocation_time);
    return info;
}

void OcspRequest::clear()
{
    _handle.reset();
}

OcspRequest::operator bool() const
{
    return _handle != nullptr;
}

} // namespace sihd::crypto
