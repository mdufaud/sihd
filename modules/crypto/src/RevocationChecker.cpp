#include <fmt/format.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/OcspRequest.hpp>
#include <sihd/crypto/RevocationChecker.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include <openssl/x509.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

std::string ocsp_cache_key(const Certificate & cert)
{
    return fmt::format("{:x}:{}", X509_issuer_name_hash(static_cast<X509 *>(cert.native())), cert.serial_hex());
}

} // namespace

RevocationChecker::RevocationChecker(): _max_cache_age(time::hours(1)) {}

void RevocationChecker::set_ocsp_fetcher(OcspFetcher fetcher)
{
    std::lock_guard lock(_mutex);
    _ocsp_fetcher = std::move(fetcher);
}

void RevocationChecker::set_crl_fetcher(CrlFetcher fetcher)
{
    std::lock_guard lock(_mutex);
    _crl_fetcher = std::move(fetcher);
}

void RevocationChecker::set_max_cache_age(Duration age)
{
    std::lock_guard lock(_mutex);
    _max_cache_age = age;
}

Result<RevocationInfo> RevocationChecker::check(const std::vector<Certificate> & chain)
{
    if (chain.size() < 2 || !chain[0] || !chain[1])
        return std::unexpected(Error(invalid_argument, "revocation check needs the certificate and its issuer"));
    const Certificate & cert = chain[0];
    const std::string key = ocsp_cache_key(cert);

    OcspFetcher ocsp_fetcher;
    CrlFetcher crl_fetcher;
    {
        std::lock_guard lock(_mutex);
        ocsp_fetcher = _ocsp_fetcher;
        crl_fetcher = _crl_fetcher;
        auto it = _ocsp_cache.find(key);
        if (it != _ocsp_cache.end() && it->second.expiry > Timestamp::now())
            return it->second.info;
    }

    std::vector<std::string> failures;
    ErrorCode last_code = none;

    if (ocsp_fetcher)
    {
        for (const std::string & url : cert.ocsp_urls())
        {
            auto info = this->_check_ocsp(ocsp_fetcher, url, chain);
            if (info.has_value())
            {
                std::lock_guard lock(_mutex);
                _ocsp_cache[key] = CachedOcsp {.info = *info, .expiry = this->_expiry(info->next_update)};
                return info;
            }
            last_code = info.error().code;
            failures.emplace_back(fmt::format("ocsp {}: {}", url, info.error().message));
        }
    }

    if (crl_fetcher)
    {
        for (const std::string & url : cert.crl_urls())
        {
            auto info = this->_check_crl(crl_fetcher, url, chain);
            if (info.has_value())
                return info;
            last_code = info.error().code;
            failures.emplace_back(fmt::format("crl {}: {}", url, info.error().message));
        }
    }

    if (failures.empty())
        return RevocationInfo {};
    return std::unexpected(Error(last_code, "{}", str::join(failures, "; ")));
}

Result<RevocationInfo> RevocationChecker::_check_ocsp(const OcspFetcher & fetcher,
                                                      const std::string & url,
                                                      const std::vector<Certificate> & chain)
{
    OcspRequest request;
    auto built = request.build(chain[0], chain[1]);
    SIHD_UNEXPECTED_RETURN(built);
    auto der = request.to_der();
    SIHD_UNEXPECTED_RETURN(der);
    auto answer = fetcher(url, *der);
    SIHD_UNEXPECTED_RETURN(answer);
    return request.verify_response(answer->data(), answer->size(), chain);
}

Result<RevocationInfo> RevocationChecker::_check_crl(const CrlFetcher & fetcher,
                                                     const std::string & url,
                                                     const std::vector<Certificate> & chain)
{
    {
        std::lock_guard lock(_mutex);
        auto it = _crl_cache.find(url);
        if (it != _crl_cache.end() && it->second.expiry > Timestamp::now())
            return it->second.crl.status(chain[0]);
    }
    auto answer = fetcher(url);
    SIHD_UNEXPECTED_RETURN(answer);
    Crl crl;
    auto loaded = crl.load_der(answer->data(), answer->size());
    SIHD_UNEXPECTED_RETURN(loaded);
    auto verified = crl.verify(chain[1]);
    SIHD_UNEXPECTED_RETURN(verified);
    {
        std::lock_guard lock(_mutex);
        _crl_cache[url] = CachedCrl {.crl = crl, .expiry = this->_expiry(crl.next_update())};
    }
    return crl.status(chain[0]);
}

Timestamp RevocationChecker::_expiry(Timestamp next_update) const
{
    const Timestamp capped(Timestamp::now().nanoseconds() + _max_cache_age.nanoseconds());
    if (next_update.nanoseconds() == 0 || next_update > capped)
        return capped;
    return next_update;
}

} // namespace sihd::crypto
