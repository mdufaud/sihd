#include <algorithm>
#include <stdexcept>

#include <sihd/http/RateLimitFilter.hpp>

namespace sihd::http
{

using sihd::util::Duration;
using sihd::util::RateLimiter;

RateLimitFilter::RateLimitFilter(size_t max_events, Duration window):
    _max_events(max_events),
    _window(window.get()),
    _max_entries(4096),
    _clock_ptr(nullptr)
{
    if (max_events == 0)
        throw std::invalid_argument("RateLimitFilter max events must be positive");
    if (_window <= 0)
        throw std::invalid_argument("RateLimitFilter window must be positive");
}

RateLimitFilter::~RateLimitFilter() = default;

void RateLimitFilter::set_max_entries(size_t max_entries)
{
    std::lock_guard lock(_mutex);
    _max_entries = max_entries;
}

void RateLimitFilter::set_clock(sihd::util::IClock *clock)
{
    std::lock_guard lock(_mutex);
    _clock_ptr = clock;
    for (auto & [ip, limiter] : _map)
        limiter.set_clock(clock);
}

bool RateLimitFilter::_evict_full_entry()
{
    auto it = std::find_if(_map.begin(), _map.end(), [](const auto & pair) { return pair.second.full(); });
    if (it == _map.end())
        return false;
    _map.erase(it);
    return true;
}

bool RateLimitFilter::on_filter_connection(const HttpFilterInfo & info)
{
    if (info.client_ip.empty())
        return true;
    std::lock_guard lock(_mutex);
    auto it = _map.find(info.client_ip);
    if (it == _map.end())
    {
        // a full bucket is an idle client: a fresh entry behaves identically - when every
        // tracked entry is active, untracked ips pass unlimited rather than wait
        if (_max_entries != 0 && _map.size() >= _max_entries && _evict_full_entry() == false)
            return true;
        it = _map.try_emplace(info.client_ip, _max_events, Duration(_window)).first;
        if (_clock_ptr != nullptr)
            it->second.set_clock(_clock_ptr);
    }
    return it->second.allow();
}

} // namespace sihd::http
