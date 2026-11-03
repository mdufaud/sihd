#ifndef __SIHD_CRYPTO_REVOCATIONINFO_HPP__
#define __SIHD_CRYPTO_REVOCATIONINFO_HPP__

#include <sihd/util/Timestamp.hpp>

namespace sihd::crypto
{

enum class RevocationStatus
{
    good,
    revoked,
    unknown
};

enum class RevocationSource
{
    none,
    ocsp,
    crl
};

struct RevocationInfo
{
        RevocationStatus status = RevocationStatus::unknown;
        RevocationSource source = RevocationSource::none;
        sihd::util::Timestamp this_update;
        // 0 when the source gives none
        sihd::util::Timestamp next_update;
        sihd::util::Timestamp revocation_time;
};

} // namespace sihd::crypto

#endif
