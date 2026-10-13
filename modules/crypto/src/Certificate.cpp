#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h> // inet_ntop
#else
# include <ws2tcpip.h>
#endif

#include <sihd/util/Result.hpp>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

struct X509Free
{
        void operator()(X509 *cert) const { X509_free(cert); }
};
struct X509ExtFree
{
        void operator()(X509_EXTENSION *ext) const { X509_EXTENSION_free(ext); }
};
struct BnFree
{
        void operator()(BIGNUM *bn) const { BN_free(bn); }
};
struct Asn1TimeFree
{
        void operator()(ASN1_TIME *time) const { ASN1_TIME_free(time); }
};

std::shared_ptr<void> make_handle(X509 *cert)
{
    return std::shared_ptr<void>(cert, X509Free {});
}

void add_name_entry(X509_NAME *name, const char *field, const std::string & value)
{
    if (value.empty())
        return;
    X509_NAME_add_entry_by_txt(name,
                               field,
                               MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>(value.c_str()),
                               -1,
                               -1,
                               0);
}

std::string name_common_name(X509_NAME *name)
{
    if (!name)
        return {};
    int len = X509_NAME_get_text_by_NID(name, NID_commonName, nullptr, 0);
    if (len < 0)
        return {};
    std::string result(static_cast<size_t>(len) + 1, '\0');
    X509_NAME_get_text_by_NID(name, NID_commonName, result.data(), static_cast<int>(result.size()));
    result.resize(static_cast<size_t>(len));
    return result;
}

sihd::util::Timestamp asn1_time_to_timestamp(const ASN1_TIME *time)
{
    if (!time)
        return sihd::util::Timestamp(0);
    std::unique_ptr<ASN1_TIME, Asn1TimeFree> epoch(ASN1_TIME_set(nullptr, 0));
    if (epoch == nullptr)
        return sihd::util::Timestamp(0);
    int days = 0;
    int secs = 0;
    int ok = ASN1_TIME_diff(&days, &secs, epoch.get(), time);
    if (ok != 1)
        return sihd::util::Timestamp(0);
    int64_t total = static_cast<int64_t>(days) * 86400 + secs;
    return sihd::util::Timestamp(sihd::util::time::seconds(total));
}

} // namespace

sihd::util::Result<void>
    Certificate::generate_self_signed(const PrivateKey & key, std::string_view common_name, int days)
{
    CertOptions opts;
    opts.common_name = std::string(common_name);
    opts.days = days;
    return this->generate_self_signed(key, opts);
}

sihd::util::Result<void> Certificate::generate_self_signed(const PrivateKey & key, const CertOptions & opts)
{
    ERR_clear_error();
    this->clear();
    if (!key)
        return std::unexpected(Error(not_initialized, "cannot generate a certificate with an empty private key"));
    std::unique_ptr<X509, X509Free> cert(X509_new());
    if (cert == nullptr)
        return make_error("X509_new");

    X509_set_version(cert.get(), 2); // X509 v3

    if (opts.serial != 0)
    {
        ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert.get()), opts.serial);
    }
    else
    {
        std::unique_ptr<BIGNUM, BnFree> bn(BN_new());
        if (bn && BN_rand(bn.get(), 64, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) == 1)
            BN_to_ASN1_INTEGER(bn.get(), X509_get_serialNumber(cert.get()));
        else
            ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    }

    X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * opts.days);

    X509_set_pubkey(cert.get(), static_cast<EVP_PKEY *>(key.native()));

    X509_NAME *name = X509_get_subject_name(cert.get());
    add_name_entry(name, "CN", opts.common_name);
    add_name_entry(name, "O", opts.organization);
    add_name_entry(name, "OU", opts.organizational_unit);
    add_name_entry(name, "C", opts.country);
    add_name_entry(name, "ST", opts.state);
    add_name_entry(name, "L", opts.locality);
    add_name_entry(name, "emailAddress", opts.email);
    X509_set_issuer_name(cert.get(), name);

    if (!opts.subject_alt_names.empty())
    {
        std::string san = sihd::util::str::join(opts.subject_alt_names, ",");
        std::unique_ptr<X509_EXTENSION, X509ExtFree> ext(
            X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, const_cast<char *>(san.c_str())));
        if (ext == nullptr)
        {
            // conf failure left the openssl queue dirty for the next make_error
            ERR_clear_error();
            return std::unexpected(Error(invalid_argument, "invalid subject alt names: {}", san));
        }
        if (X509_add_ext(cert.get(), ext.get(), -1) != 1)
        {
            // add failure left the openssl queue dirty for the next make_error
            ERR_clear_error();
            return make_error("could not add subject alt names");
        }
    }

    if (opts.is_ca)
    {
        std::unique_ptr<X509_EXTENSION, X509ExtFree> ext(
            X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, const_cast<char *>("critical,CA:TRUE")));
        if (ext)
        {
            if (X509_add_ext(cert.get(), ext.get(), -1) != 1)
            {
                // add failure left the openssl queue dirty for the next make_error
                ERR_clear_error();
                return make_error("could not add basic constraints");
            }
        }
        else
        {
            // ignored failure: drain the queue, it must not leak into the next make_error
            ERR_clear_error();
        }
    }

    if (X509_sign(cert.get(), static_cast<EVP_PKEY *>(key.native()), EVP_sha256()) <= 0)
        return make_error("X509_sign");
    _handle = make_handle(cert.release());
    return {};
}

sihd::util::Result<void> Certificate::load_pem(std::string_view path)
{
    ERR_clear_error();
    this->clear();
    FILE *fp = fopen(std::string(path).c_str(), "r");
    if (!fp)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    std::unique_ptr<X509, X509Free> cert(PEM_read_X509(fp, nullptr, nullptr, nullptr));
    fclose(fp);
    if (cert == nullptr)
        return make_error("read PEM from '{}'", path);
    _handle = make_handle(cert.release());
    return {};
}

sihd::util::Result<void> Certificate::save_pem(std::string_view path) const
{
    ERR_clear_error();
    if (!_handle)
        return std::unexpected(Error(not_initialized, "no certificate loaded"));
    FILE *fp = fopen(std::string(path).c_str(), "w");
    if (!fp)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    const bool ok = PEM_write_X509(fp, static_cast<X509 *>(_handle.get())) > 0;
    fclose(fp);
    if (!ok)
        return make_error("write PEM to '{}'", path);
    return {};
}

sihd::util::Result<void> Certificate::load_der(const uint8_t *data, size_t len)
{
    ERR_clear_error();
    this->clear();
    const unsigned char *p = data;
    std::unique_ptr<X509, X509Free> cert(d2i_X509(nullptr, &p, static_cast<long>(len)));
    if (cert == nullptr)
        return make_error("read DER data");
    _handle = make_handle(cert.release());
    return {};
}

sihd::util::Result<void> Certificate::load_pem_string(std::string_view pem)
{
    ERR_clear_error();
    this->clear();
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        return make_error("BIO_new_mem_buf");
    std::unique_ptr<X509, X509Free> cert(PEM_read_bio_X509(bio, nullptr, nullptr, nullptr));
    BIO_free(bio);
    if (cert == nullptr)
        return make_error("read PEM from string");
    _handle = make_handle(cert.release());
    return {};
}

std::string Certificate::to_pem_string() const
{
    if (!_handle)
        return {};
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};
    if (PEM_write_bio_X509(bio, static_cast<X509 *>(_handle.get())) <= 0)
    {
        BIO_free(bio);
        return {};
    }
    char *data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string result(data, static_cast<size_t>(len));
    BIO_free(bio);
    return result;
}

bool Certificate::verify(const Certificate & ca) const
{
    if (!_handle || !ca._handle)
        return false;
    EVP_PKEY *ca_key = X509_get0_pubkey(static_cast<X509 *>(ca._handle.get()));
    if (!ca_key)
        return false;
    return X509_verify(static_cast<X509 *>(_handle.get()), ca_key) == 1;
}

std::string Certificate::subject_common_name() const
{
    if (!_handle)
        return {};
    return name_common_name(X509_get_subject_name(static_cast<X509 *>(_handle.get())));
}

std::string Certificate::issuer_common_name() const
{
    if (!_handle)
        return {};
    return name_common_name(X509_get_issuer_name(static_cast<X509 *>(_handle.get())));
}

std::vector<std::string> Certificate::subject_alt_names() const
{
    std::vector<std::string> result;
    if (!_handle)
        return result;
    auto *names = static_cast<GENERAL_NAMES *>(
        X509_get_ext_d2i(static_cast<X509 *>(_handle.get()), NID_subject_alt_name, nullptr, nullptr));
    if (!names)
        return result;
    int count = sk_GENERAL_NAME_num(names);
    for (int i = 0; i < count; ++i)
    {
        const GENERAL_NAME *gen = sk_GENERAL_NAME_value(names, i);
        if (gen->type == GEN_DNS)
        {
            const ASN1_IA5STRING *dns = gen->d.dNSName;
            result.emplace_back("DNS:"
                                + std::string(reinterpret_cast<const char *>(ASN1_STRING_get0_data(dns)),
                                              static_cast<size_t>(ASN1_STRING_length(dns))));
        }
        else if (gen->type == GEN_IPADD)
        {
            const ASN1_OCTET_STRING *ip = gen->d.iPAddress;
            const unsigned char *data = ASN1_STRING_get0_data(ip);
            int len = ASN1_STRING_length(ip);
            if (len == 4)
            {
                result.push_back(sihd::util::str::format("IP:%u.%u.%u.%u", data[0], data[1], data[2], data[3]));
            }
            else if (len == 16)
            {
                char buf[INET6_ADDRSTRLEN] = {0};
                if (inet_ntop(AF_INET6, data, buf, sizeof(buf)))
                    result.emplace_back(std::string("IP:") + buf);
            }
        }
    }
    GENERAL_NAMES_free(names);
    return result;
}

sihd::util::Timestamp Certificate::not_before() const
{
    if (!_handle)
        return sihd::util::Timestamp(0);
    return asn1_time_to_timestamp(X509_get0_notBefore(static_cast<X509 *>(_handle.get())));
}

sihd::util::Timestamp Certificate::not_after() const
{
    if (!_handle)
        return sihd::util::Timestamp(0);
    return asn1_time_to_timestamp(X509_get0_notAfter(static_cast<X509 *>(_handle.get())));
}

std::string Certificate::serial_hex() const
{
    if (!_handle)
        return {};
    ASN1_INTEGER *serial = X509_get_serialNumber(static_cast<X509 *>(_handle.get()));
    if (!serial)
        return {};
    BIGNUM *bn = ASN1_INTEGER_to_BN(serial, nullptr);
    if (!bn)
        return {};
    char *hex = BN_bn2hex(bn);
    std::string result = hex ? hex : "";
    OPENSSL_free(hex);
    BN_free(bn);
    return result;
}

bool Certificate::is_ca() const
{
    if (!_handle)
        return false;
    return X509_check_ca(static_cast<X509 *>(_handle.get())) > 0;
}

void Certificate::clear()
{
    _handle.reset();
}

} // namespace sihd::crypto
