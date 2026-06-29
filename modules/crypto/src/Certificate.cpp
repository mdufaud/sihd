#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h> // inet_ntop
#else
# include <ws2tcpip.h>
#endif

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace sihd::crypto
{

SIHD_LOGGER;

namespace
{

X509 *as_cert(void *h) { return static_cast<X509 *>(h); }
EVP_PKEY *as_key(void *h) { return static_cast<EVP_PKEY *>(h); }

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
    ASN1_TIME *epoch = ASN1_TIME_set(nullptr, 0);
    if (!epoch)
        return sihd::util::Timestamp(0);
    int days = 0;
    int secs = 0;
    int ok = ASN1_TIME_diff(&days, &secs, epoch, time);
    ASN1_TIME_free(epoch);
    if (ok != 1)
        return sihd::util::Timestamp(0);
    int64_t total = static_cast<int64_t>(days) * 86400 + secs;
    return sihd::util::Timestamp(sihd::util::time::seconds(total));
}

} // namespace

Certificate::Certificate(): _handle(nullptr) {}

Certificate::~Certificate() { this->clear(); }

Certificate::Certificate(const Certificate & other): _handle(nullptr)
{
    if (other._handle)
    {
        X509_up_ref(as_cert(other._handle));
        _handle = other._handle;
    }
}

Certificate & Certificate::operator=(const Certificate & other)
{
    if (this != &other)
    {
        this->clear();
        if (other._handle)
        {
            X509_up_ref(as_cert(other._handle));
            _handle = other._handle;
        }
    }
    return *this;
}

Certificate::Certificate(Certificate && other) noexcept: _handle(other._handle)
{
    other._handle = nullptr;
}

Certificate & Certificate::operator=(Certificate && other) noexcept
{
    if (this != &other)
    {
        this->clear();
        _handle = other._handle;
        other._handle = nullptr;
    }
    return *this;
}

bool Certificate::generate_self_signed(const PrivateKey & key, std::string_view common_name, int days)
{
    CertOptions opts;
    opts.common_name = std::string(common_name);
    opts.days = days;
    return this->generate_self_signed(key, opts);
}

bool Certificate::generate_self_signed(const PrivateKey & key, const CertOptions & opts)
{
    this->clear();
    if (!key)
    {
        SIHD_LOG(error, "Certificate: key is empty");
        return false;
    }
    X509 *cert = X509_new();
    if (!cert)
        return false;

    X509_set_version(cert, 2); // X509 v3

    if (opts.serial != 0)
    {
        ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert), opts.serial);
    }
    else
    {
        BIGNUM *bn = BN_new();
        if (bn && BN_rand(bn, 64, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) == 1)
            BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(cert));
        else
            ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
        BN_free(bn);
    }

    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 60L * 60 * 24 * opts.days);

    X509_set_pubkey(cert, as_key(key.native()));

    X509_NAME *name = X509_get_subject_name(cert);
    add_name_entry(name, "CN", opts.common_name);
    add_name_entry(name, "O", opts.organization);
    add_name_entry(name, "OU", opts.organizational_unit);
    add_name_entry(name, "C", opts.country);
    add_name_entry(name, "ST", opts.state);
    add_name_entry(name, "L", opts.locality);
    add_name_entry(name, "emailAddress", opts.email);
    X509_set_issuer_name(cert, name);

    if (!opts.subject_alt_names.empty())
    {
        std::string san = sihd::util::str::join(opts.subject_alt_names, ",");
        X509_EXTENSION *ext
            = X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, const_cast<char *>(san.c_str()));
        if (ext)
        {
            X509_add_ext(cert, ext, -1);
            X509_EXTENSION_free(ext);
        }
        else
        {
            SIHD_LOG(error, "Certificate: invalid subject alt names: {}", san);
        }
    }

    if (opts.is_ca)
    {
        X509_EXTENSION *ext
            = X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, const_cast<char *>("critical,CA:TRUE"));
        if (ext)
        {
            X509_add_ext(cert, ext, -1);
            X509_EXTENSION_free(ext);
        }
    }

    if (X509_sign(cert, as_key(key.native()), EVP_sha256()) <= 0)
    {
        SIHD_LOG(error, "Certificate: signing failed");
        X509_free(cert);
        return false;
    }
    _handle = cert;
    return true;
}

bool Certificate::load_pem(std::string_view path)
{
    this->clear();
    FILE *fp = fopen(std::string(path).c_str(), "r");
    if (!fp)
    {
        SIHD_LOG(error, "Certificate: cannot open file: {}", path);
        return false;
    }
    X509 *cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!cert)
    {
        SIHD_LOG(error, "Certificate: failed to read PEM from: {}", path);
        return false;
    }
    _handle = cert;
    return true;
}

bool Certificate::save_pem(std::string_view path) const
{
    if (!_handle)
        return false;
    FILE *fp = fopen(std::string(path).c_str(), "w");
    if (!fp)
    {
        SIHD_LOG(error, "Certificate: cannot open file for writing: {}", path);
        return false;
    }
    bool ok = PEM_write_X509(fp, as_cert(_handle)) > 0;
    fclose(fp);
    return ok;
}

bool Certificate::load_der(const uint8_t *data, size_t len)
{
    this->clear();
    const unsigned char *p = data;
    X509 *cert = d2i_X509(nullptr, &p, static_cast<long>(len));
    if (!cert)
    {
        SIHD_LOG(error, "Certificate: failed to read DER data");
        return false;
    }
    _handle = cert;
    return true;
}

bool Certificate::load_pem_string(std::string_view pem)
{
    this->clear();
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        return false;
    X509 *cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!cert)
    {
        SIHD_LOG(error, "Certificate: failed to read PEM from string");
        return false;
    }
    _handle = cert;
    return true;
}

std::string Certificate::to_pem_string() const
{
    if (!_handle)
        return {};
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};
    if (PEM_write_bio_X509(bio, as_cert(_handle)) <= 0)
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
    EVP_PKEY *ca_key = X509_get0_pubkey(as_cert(ca._handle));
    if (!ca_key)
        return false;
    return X509_verify(as_cert(_handle), ca_key) == 1;
}

std::string Certificate::subject_common_name() const
{
    if (!_handle)
        return {};
    return name_common_name(X509_get_subject_name(as_cert(_handle)));
}

std::string Certificate::issuer_common_name() const
{
    if (!_handle)
        return {};
    return name_common_name(X509_get_issuer_name(as_cert(_handle)));
}

std::vector<std::string> Certificate::subject_alt_names() const
{
    std::vector<std::string> result;
    if (!_handle)
        return result;
    auto *names = static_cast<GENERAL_NAMES *>(
        X509_get_ext_d2i(as_cert(_handle), NID_subject_alt_name, nullptr, nullptr));
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
    return asn1_time_to_timestamp(X509_get0_notBefore(as_cert(_handle)));
}

sihd::util::Timestamp Certificate::not_after() const
{
    if (!_handle)
        return sihd::util::Timestamp(0);
    return asn1_time_to_timestamp(X509_get0_notAfter(as_cert(_handle)));
}

std::string Certificate::serial_hex() const
{
    if (!_handle)
        return {};
    ASN1_INTEGER *serial = X509_get_serialNumber(as_cert(_handle));
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
    return X509_check_ca(as_cert(_handle)) > 0;
}

void Certificate::clear()
{
    if (_handle)
    {
        X509_free(as_cert(_handle));
        _handle = nullptr;
    }
}

} // namespace sihd::crypto
