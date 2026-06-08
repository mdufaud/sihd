#ifndef __SIHD_UTIL_CACHE_HPP__
#define __SIHD_UTIL_CACHE_HPP__

#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::util
{

template <typename Key, typename Value>
class Cache
{
    public:
        using CacheGetter = std::function<Value()>;

        struct Stats
        {
                size_t hit = 0;
                size_t miss = 0;
        };

        Cache() = default;
        ~Cache() = default;

        /** Sets a value in the cache.
         * @param key The key for the value.
         * @param getter The function to call to get the value.
         * @param max_age The maximum age of the cached value.
         * @param lazy Whether to lazily initialize the cached value.
         */
        void set(const Key & key, CacheGetter getter, Timestamp max_age = -1, bool lazy = false)
        {
            if (lazy)
            {
                _cache[key] = CacheEntry {.max_age = max_age, .getter = std::move(getter), .cached_value = Value()};
                return;
            }

            Value cached_value = getter();
            _cache[key] = CacheEntry {.last_refresh = _clock.now(),
                                      .max_age = max_age,
                                      .getter = std::move(getter),
                                      .cached_value = std::move(cached_value)};
        }

        // This method throws if the key is missing.
        Value & get(const Key & key, Timestamp ttl = -1)
        {
            auto & entry = _get_cache(key);
            if (this->_needs_refresh(entry, ttl))
            {
                entry.miss++;
                entry.cached_value = entry.getter();
                entry.last_refresh = _clock.now();
            }
            else
            {
                entry.hit++;
            }

            return entry.cached_value;
        }

        // This method does not throw if the key is missing, it returns std::nullopt instead.
        std::optional<std::reference_wrapper<Value>> get_optional(const Key & key, Timestamp ttl = -1)
        {
            try
            {
                return std::make_optional<std::reference_wrapper<Value>>(this->get(key, ttl));
            }
            catch (const std::out_of_range &)
            {
                return std::nullopt;
            }
        }

        // This method does not refresh the cache, it only returns the cached value if it exists.
        std::optional<std::reference_wrapper<Value>> get_cached(const Key & key)
        {
            try
            {
                auto & entry = this->_get_cache(key);
                return std::make_optional<std::reference_wrapper<Value>>(entry.cached_value);
            }
            catch (const std::out_of_range &)
            {
                return std::nullopt;
            }
        }

        void refresh_stale()
        {
            for (auto & [_, entry] : _cache)
            {
                const Timestamp now = _clock.now();
                if (this->_needs_refresh(entry, -1, now))
                {
                    entry.cached_value = entry.getter();
                    entry.last_refresh = now;
                }
            }
        }

        void refresh_all()
        {
            for (auto & [_, entry] : _cache)
            {
                entry.cached_value = entry.getter();
                entry.last_refresh = _clock.now();
            }
        }

        void invalidate(const Key & key)
        {
            auto & entry = this->_get_cache(key);
            entry.last_refresh = -1;
        }

        void invalidate_all()
        {
            for (auto & [_, entry] : _cache)
            {
                entry.last_refresh = -1;
            }
        }

        void clear() { _cache.clear(); }
        void erase(const Key & key) { _cache.erase(key); }

        size_t size() const { return _cache.size(); }
        bool contains(const Key & key) const { return _cache.find(key) != _cache.end(); }

        std::optional<Stats> entry_stats(const Key & key) const
        {
            auto it = _cache.find(key);
            if (it == _cache.end())
                return std::nullopt;

            auto & entry = it->second;
            return Stats {.hit = entry.hit, .miss = entry.miss};
        }

        Stats stats() const
        {
            Stats result;
            for (const auto & [_, entry] : _cache)
            {
                result.hit += entry.hit;
                result.miss += entry.miss;
            }
            return result;
        }

        std::vector<Key> stale_entries(Timestamp ttl = -1) const
        {
            std::vector<Key> result;
            const Timestamp now = _clock.now();
            for (const auto & [key, entry] : _cache)
            {
                if (this->_needs_refresh(entry, ttl, now))
                {
                    result.emplace_back(key);
                }
            }
            return result;
        }

    private:
        struct CacheEntry
        {
                // Stats
                size_t hit = 0;
                size_t miss = 0;
                // Time management
                Timestamp last_refresh = -1;
                Timestamp max_age = -1;
                // Value management
                std::function<Value()> getter;
                Value cached_value;
        };

        bool _needs_refresh(const CacheEntry & entry, Timestamp ttl, Timestamp now = -1) const
        {
            now = now < 0 ? Timestamp(_clock.now()) : now;
            const bool was_never_refreshed = (entry.last_refresh < 0);
            const bool is_expired_by_ttl = (ttl >= 0) && ((now - entry.last_refresh) > ttl);
            const bool is_expired_by_max_age = (entry.max_age >= 0) && ((now - entry.last_refresh) > entry.max_age);
            return was_never_refreshed || is_expired_by_ttl || is_expired_by_max_age;
        }

        [[noreturn]] void _throw_missing_key(const Key & key)
        {
            std::string exception_str = "No cache key for: ";
            if constexpr (traits::CanConcatWithLiteral<Key>)
            {
                exception_str += key;
            }
            else if constexpr (std::is_constructible_v<std::string, Key>)
            {
                exception_str += std::string(key);
            }
            else if constexpr (std::is_arithmetic_v<Key>)
            {
                exception_str += std::to_string(key);
            }
            else
            {
                exception_str += " <not printable key type>";
            }
            throw std::out_of_range(exception_str);
        }

        CacheEntry & _get_cache(const Key & key)
        {
            auto it = _cache.find(key);
            if (it == _cache.end())
                _throw_missing_key(key);
            return it->second;
        }

        std::unordered_map<Key, CacheEntry> _cache;
        SteadyClock _clock;
};

} // namespace sihd::util

#endif
