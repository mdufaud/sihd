#ifndef __SIHD_HTTP_RATELIMITFILTER_HPP__
#define __SIHD_HTTP_RATELIMITFILTER_HPP__

#include <cstddef>
#include <mutex>
#include <unordered_map>

#include <sihd/http/IHttpFilter.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/RateLimiter.hpp>
#include <sihd/util/time.hpp>

namespace sihd::http
{

// per client ip request budget, rejected requests answer 403 - safe to call from the
// server service threads
class RateLimitFilter: public IHttpFilter
{
    public:
        RateLimitFilter(size_t max_events, sihd::util::Duration window);
        ~RateLimitFilter();

        // tracked client ips bound, 0 disables the bound
        void set_max_entries(size_t max_entries);
        void set_clock(sihd::util::IClock *clock);

        bool on_filter_connection(const HttpFilterInfo & info) override;

    private:
        bool _evict_full_entry();

        size_t _max_events;
        sihd::util::time::UnixTime _window;
        size_t _max_entries;
        sihd::util::IClock *_clock_ptr;
        std::mutex _mutex;
        std::unordered_map<std::string, sihd::util::RateLimiter> _map;
};

} // namespace sihd::http

#endif
