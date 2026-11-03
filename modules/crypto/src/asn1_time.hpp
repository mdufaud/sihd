#ifndef __SIHD_CRYPTO_ASN1_TIME_HPP__
#define __SIHD_CRYPTO_ASN1_TIME_HPP__

#include <sihd/util/Timestamp.hpp>

#include <openssl/asn1.h>

namespace sihd::crypto
{

// 0 when the time is absent or unreadable
sihd::util::Timestamp asn1_time_to_timestamp(const ASN1_TIME *time);

} // namespace sihd::crypto

#endif
