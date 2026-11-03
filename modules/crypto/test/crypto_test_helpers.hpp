#ifndef __CRYPTO_TEST_HELPERS_HPP__
#define __CRYPTO_TEST_HELPERS_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>

#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace test
{

struct OcspAnswer
{
        int status = V_OCSP_CERTSTATUS_GOOD;
        bool echo_nonce = true;
        bool foreign_nonce = false;
        long this_update_offset = 0;
        long next_update_offset = 3600;
};

struct CrlContent
{
        std::vector<const sihd::crypto::Certificate *> revoked = {};
        long this_update_offset = 0;
        bool has_next_update = true;
        long next_update_offset = 3600;
        std::vector<std::string> partition_urls = {};
        bool delta = false;
};

inline sihd::crypto::PrivateKey generate_rsa_key()
{
    sihd::crypto::PrivateKey key;
    (void)key.generate_rsa(2048);
    return key;
}

// rsa generation dominates the suite: generated once per process, copies share the EVP_PKEY
inline const sihd::crypto::PrivateKey & rsa_key()
{
    static const sihd::crypto::PrivateKey key = generate_rsa_key();
    return key;
}

inline const sihd::crypto::PrivateKey & other_rsa_key()
{
    static const sihd::crypto::PrivateKey key = generate_rsa_key();
    return key;
}

inline sihd::crypto::Certificate make_ca(const sihd::crypto::PrivateKey & key, const std::string & name)
{
    sihd::crypto::CertOptions opts;
    opts.common_name = name;
    opts.is_ca = true;
    sihd::crypto::Certificate ca;
    (void)ca.generate_self_signed(key, opts);
    return ca;
}

inline sihd::crypto::Certificate make_leaf(const sihd::crypto::Certificate & ca,
                                           const sihd::crypto::PrivateKey & ca_key,
                                           const sihd::crypto::PrivateKey & leaf_key,
                                           const sihd::crypto::CertOptions & opts)
{
    sihd::crypto::Certificate leaf;
    (void)leaf.generate_signed(leaf_key, opts, ca, ca_key);
    return leaf;
}

inline void add_ocsp_signing_usage(sihd::crypto::Certificate & cert, const sihd::crypto::PrivateKey & ca_key)
{
    auto *x509 = static_cast<X509 *>(cert.native());
    X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_ext_key_usage, "OCSPSigning");
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);
    X509_sign(x509, static_cast<EVP_PKEY *>(ca_key.native()), EVP_sha256());
}

template <typename Encode, typename T>
std::vector<uint8_t> to_der(Encode encode, T *object)
{
    std::vector<uint8_t> der(static_cast<size_t>(encode(object, nullptr)));
    unsigned char *out = der.data();
    encode(object, &out);
    return der;
}

inline std::vector<uint8_t> ocsp_respond(const std::vector<uint8_t> & request_der,
                                         const sihd::crypto::Certificate & signer,
                                         const sihd::crypto::PrivateKey & signer_key,
                                         const OcspAnswer & answer = {})
{
    const unsigned char *in = request_der.data();
    OCSP_REQUEST *request = d2i_OCSP_REQUEST(nullptr, &in, static_cast<long>(request_der.size()));
    OCSP_CERTID *id = OCSP_onereq_get0_id(OCSP_request_onereq_get0(request, 0));

    OCSP_BASICRESP *basic = OCSP_BASICRESP_new();
    ASN1_TIME *this_update = X509_gmtime_adj(nullptr, answer.this_update_offset);
    ASN1_TIME *next_update = X509_gmtime_adj(nullptr, answer.next_update_offset);
    const bool revoked = answer.status == V_OCSP_CERTSTATUS_REVOKED;
    ASN1_TIME *revocation_time = revoked ? X509_gmtime_adj(nullptr, -3600) : nullptr;
    OCSP_basic_add1_status(basic,
                           id,
                           answer.status,
                           revoked ? OCSP_REVOKED_STATUS_KEYCOMPROMISE : -1,
                           revocation_time,
                           this_update,
                           next_update);
    if (answer.echo_nonce)
        OCSP_copy_nonce(basic, request);
    if (answer.foreign_nonce)
        OCSP_basic_add1_nonce(basic, nullptr, -1);
    OCSP_basic_sign(basic,
                    static_cast<X509 *>(signer.native()),
                    static_cast<EVP_PKEY *>(signer_key.native()),
                    EVP_sha256(),
                    nullptr,
                    0);

    OCSP_RESPONSE *response = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, basic);
    std::vector<uint8_t> der = to_der(i2d_OCSP_RESPONSE, response);
    OCSP_RESPONSE_free(response);
    ASN1_TIME_free(this_update);
    ASN1_TIME_free(next_update);
    ASN1_TIME_free(revocation_time);
    OCSP_BASICRESP_free(basic);
    OCSP_REQUEST_free(request);
    return der;
}

inline std::vector<uint8_t> ocsp_refuse(int response_status)
{
    OCSP_RESPONSE *response = OCSP_response_create(response_status, nullptr);
    std::vector<uint8_t> der = to_der(i2d_OCSP_RESPONSE, response);
    OCSP_RESPONSE_free(response);
    return der;
}

inline void add_crl_partition(X509_CRL *crl, const std::vector<std::string> & urls)
{
    ISSUING_DIST_POINT *idp = ISSUING_DIST_POINT_new();
    idp->distpoint = DIST_POINT_NAME_new();
    idp->distpoint->type = 0;
    idp->distpoint->name.fullname = GENERAL_NAMES_new();
    for (const std::string & url : urls)
    {
        ASN1_IA5STRING *uri = ASN1_IA5STRING_new();
        ASN1_STRING_set(uri, url.data(), static_cast<int>(url.size()));
        GENERAL_NAME *name = GENERAL_NAME_new();
        GENERAL_NAME_set0_value(name, GEN_URI, uri);
        sk_GENERAL_NAME_push(idp->distpoint->name.fullname, name);
    }
    X509_CRL_add1_ext_i2d(crl, NID_issuing_distribution_point, idp, 1, 0);
    ISSUING_DIST_POINT_free(idp);
}

inline std::vector<uint8_t> make_crl_der(const sihd::crypto::Certificate & issuer,
                                         const sihd::crypto::PrivateKey & issuer_key,
                                         const CrlContent & content = {})
{
    X509_CRL *crl = X509_CRL_new();
    X509_CRL_set_version(crl, 1);
    X509_CRL_set_issuer_name(crl, X509_get_subject_name(static_cast<X509 *>(issuer.native())));
    ASN1_TIME *this_update = X509_gmtime_adj(nullptr, content.this_update_offset);
    ASN1_TIME *next_update = X509_gmtime_adj(nullptr, content.next_update_offset);
    X509_CRL_set1_lastUpdate(crl, this_update);
    if (content.has_next_update)
        X509_CRL_set1_nextUpdate(crl, next_update);
    for (const sihd::crypto::Certificate *cert : content.revoked)
    {
        X509_REVOKED *entry = X509_REVOKED_new();
        X509_REVOKED_set_serialNumber(entry, X509_get_serialNumber(static_cast<X509 *>(cert->native())));
        X509_REVOKED_set_revocationDate(entry, this_update);
        X509_CRL_add0_revoked(crl, entry);
    }
    if (!content.partition_urls.empty())
        add_crl_partition(crl, content.partition_urls);
    if (content.delta)
    {
        ASN1_INTEGER *base = ASN1_INTEGER_new();
        ASN1_INTEGER_set(base, 1);
        X509_CRL_add1_ext_i2d(crl, NID_delta_crl, base, 1, 0);
        ASN1_INTEGER_free(base);
    }
    X509_CRL_sort(crl);
    X509_CRL_sign(crl, static_cast<EVP_PKEY *>(issuer_key.native()), EVP_sha256());

    std::vector<uint8_t> der = to_der(i2d_X509_CRL, crl);
    ASN1_TIME_free(this_update);
    ASN1_TIME_free(next_update);
    X509_CRL_free(crl);
    return der;
}

} // namespace test

#endif
