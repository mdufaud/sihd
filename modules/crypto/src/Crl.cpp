#include <algorithm>
#include <string>
#include <vector>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/Crl.hpp>
#include <sihd/crypto/error.hpp>

#include "asn1_time.hpp"
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

struct X509CrlFree
{
        void operator()(X509_CRL *crl) const { X509_CRL_free(crl); }
};
struct IdpFree
{
        void operator()(ISSUING_DIST_POINT *idp) const { ISSUING_DIST_POINT_free(idp); }
};

// a partitioned CRL speaks only for the certificates whose distribution point names its partition
bool partition_covers(const ISSUING_DIST_POINT *idp, const Certificate & cert)
{
    if (idp->onlysomereasons != nullptr || idp->onlyattr != 0 || idp->indirectCRL != 0)
        return false;
    if ((idp->onlyuser != 0 && cert.is_ca()) || (idp->onlyCA != 0 && !cert.is_ca()))
        return false;
    if (idp->distpoint == nullptr || idp->distpoint->type != 0)
        return idp->distpoint == nullptr;
    const std::vector<std::string> cert_urls = cert.crl_urls();
    const GENERAL_NAMES *names = idp->distpoint->name.fullname;
    for (int i = 0; i < sk_GENERAL_NAME_num(names); ++i)
    {
        const GENERAL_NAME *gen = sk_GENERAL_NAME_value(names, i);
        if (gen->type != GEN_URI)
            continue;
        const ASN1_IA5STRING *uri = gen->d.uniformResourceIdentifier;
        const std::string url(reinterpret_cast<const char *>(ASN1_STRING_get0_data(uri)),
                              static_cast<size_t>(ASN1_STRING_length(uri)));
        if (std::find(cert_urls.begin(), cert_urls.end(), url) != cert_urls.end())
            return true;
    }
    return false;
}

} // namespace

Result<void> Crl::load_der(const uint8_t *data, size_t len)
{
    ERR_clear_error();
    const unsigned char *in = data;
    std::unique_ptr<X509_CRL, X509CrlFree> crl(d2i_X509_CRL(nullptr, &in, static_cast<long>(len)));
    if (crl == nullptr)
        return make_error("read CRL DER data");
    _handle = std::shared_ptr<void>(crl.release(), X509CrlFree {});
    return {};
}

Result<void> Crl::verify(const Certificate & issuer) const
{
    ERR_clear_error();
    if (!_handle || !issuer)
        return std::unexpected(Error(not_initialized, "cannot verify a CRL without CRL and issuer"));
    auto *crl = static_cast<X509_CRL *>(_handle.get());
    auto *issuer_cert = static_cast<X509 *>(issuer.native());
    if (X509_NAME_cmp(X509_CRL_get_issuer(crl), X509_get_subject_name(issuer_cert)) != 0)
        return std::unexpected(Error(invalid_argument, "CRL was not issued by '{}'", issuer.subject_common_name()));
    EVP_PKEY *issuer_key = X509_get0_pubkey(issuer_cert);
    if (issuer_key == nullptr || X509_CRL_verify(crl, issuer_key) != 1)
    {
        ERR_clear_error();
        return std::unexpected(
            Error(invalid_argument, "CRL signature does not match '{}'", issuer.subject_common_name()));
    }
    if (X509_CRL_get_ext_by_NID(crl, NID_delta_crl, -1) >= 0)
        return std::unexpected(Error(not_supported, "delta CRLs are not supported"));
    const int64_t now = Timestamp::now().seconds();
    const Timestamp next = this->next_update();
    if (this->this_update().seconds() > now + validity_leeway_seconds
        || (next.seconds() != 0 && next.seconds() < now - validity_leeway_seconds))
        return std::unexpected(Error(invalid_argument, "CRL is outside its validity period"));
    return {};
}

Result<RevocationInfo> Crl::status(const Certificate & cert) const
{
    if (!_handle || !cert)
        return std::unexpected(Error(not_initialized, "cannot look up a certificate without CRL and certificate"));
    auto *crl = static_cast<X509_CRL *>(_handle.get());
    auto *x509 = static_cast<X509 *>(cert.native());

    RevocationInfo info;
    info.source = RevocationSource::crl;
    info.this_update = this->this_update();
    info.next_update = this->next_update();
    if (X509_NAME_cmp(X509_CRL_get_issuer(crl), X509_get_issuer_name(x509)) != 0)
        return info;
    std::unique_ptr<ISSUING_DIST_POINT, IdpFree> idp(
        static_cast<ISSUING_DIST_POINT *>(X509_CRL_get_ext_d2i(crl, NID_issuing_distribution_point, nullptr, nullptr)));
    if (idp != nullptr && !partition_covers(idp.get(), cert))
        return info;
    X509_REVOKED *revoked = nullptr;
    // 1 revokes; 2 is a removeFromCRL entry lifting a hold
    if (X509_CRL_get0_by_cert(crl, &revoked, x509) == 1)
    {
        info.status = RevocationStatus::revoked;
        info.revocation_time = asn1_time_to_timestamp(X509_REVOKED_get0_revocationDate(revoked));
    }
    else
    {
        info.status = RevocationStatus::good;
    }
    return info;
}

Timestamp Crl::this_update() const
{
    if (!_handle)
        return Timestamp(0);
    return asn1_time_to_timestamp(X509_CRL_get0_lastUpdate(static_cast<X509_CRL *>(_handle.get())));
}

Timestamp Crl::next_update() const
{
    if (!_handle)
        return Timestamp(0);
    return asn1_time_to_timestamp(X509_CRL_get0_nextUpdate(static_cast<X509_CRL *>(_handle.get())));
}

void Crl::clear()
{
    _handle.reset();
}

Crl::operator bool() const
{
    return _handle != nullptr;
}

} // namespace sihd::crypto
