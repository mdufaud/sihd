#include "asn1_time.hpp"

#include <memory>

#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace sihd::crypto
{

namespace
{

struct Asn1TimeFree
{
        void operator()(ASN1_TIME *time) const { ASN1_TIME_free(time); }
};

} // namespace

Timestamp asn1_time_to_timestamp(const ASN1_TIME *time)
{
    if (!time)
        return Timestamp(0);
    std::unique_ptr<ASN1_TIME, Asn1TimeFree> epoch(ASN1_TIME_set(nullptr, 0));
    if (epoch == nullptr)
        return Timestamp(0);
    int days = 0;
    int secs = 0;
    int ok = ASN1_TIME_diff(&days, &secs, epoch.get(), time);
    if (ok != 1)
        return Timestamp(0);
    int64_t total = static_cast<int64_t>(days) * 86400 + secs;
    return Timestamp(time::seconds(total));
}

} // namespace sihd::crypto
