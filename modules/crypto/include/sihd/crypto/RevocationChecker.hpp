#ifndef __SIHD_CRYPTO_REVOCATIONCHECKER_HPP__
#define __SIHD_CRYPTO_REVOCATIONCHECKER_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <sihd/crypto/Crl.hpp>
#include <sihd/crypto/RevocationInfo.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::crypto
{

class Certificate;

class RevocationChecker
{
    public:
        // POSTs the DER OCSP request to url, returns the DER OCSP response
        using OcspFetcher = std::function<sihd::util::Result<std::vector<uint8_t>>(const std::string & url,
                                                                                   sihd::util::ArrUByteView request)>;
        // GETs url, returns the DER CRL
        using CrlFetcher = std::function<sihd::util::Result<std::vector<uint8_t>>(const std::string & url)>;

        RevocationChecker();
        ~RevocationChecker() = default;

        RevocationChecker(const RevocationChecker &) = delete;
        RevocationChecker & operator=(const RevocationChecker &) = delete;

        // unset: OCSP responders are skipped
        void set_ocsp_fetcher(OcspFetcher fetcher);
        // unset: CRLs are skipped
        void set_crl_fetcher(CrlFetcher fetcher);
        // caps the reuse of an answer, which otherwise lasts until its nextUpdate
        void set_max_cache_age(sihd::util::Duration age);

        // chain: certificate, issuer, ..., trust anchor; an error only when every named source failed
        sihd::util::Result<RevocationInfo> check(const std::vector<Certificate> & chain);

    private:
        struct CachedOcsp
        {
                RevocationInfo info;
                sihd::util::Timestamp expiry;
        };

        struct CachedCrl
        {
                Crl crl;
                sihd::util::Timestamp expiry;
        };

        sihd::util::Result<RevocationInfo>
            _check_ocsp(const OcspFetcher & fetcher, const std::string & url, const std::vector<Certificate> & chain);
        sihd::util::Result<RevocationInfo>
            _check_crl(const CrlFetcher & fetcher, const std::string & url, const std::vector<Certificate> & chain);
        sihd::util::Timestamp _expiry(sihd::util::Timestamp next_update) const;

        std::mutex _mutex;
        OcspFetcher _ocsp_fetcher;
        CrlFetcher _crl_fetcher;
        sihd::util::Duration _max_cache_age;
        std::map<std::string, CachedOcsp> _ocsp_cache;
        std::map<std::string, CachedCrl> _crl_cache;
};

} // namespace sihd::crypto

#endif
