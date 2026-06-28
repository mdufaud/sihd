#ifndef __SIHD_UTIL_SLICE_HPP__
#define __SIHD_UTIL_SLICE_HPP__

#include <algorithm>
#include <cstddef>

namespace sihd::util
{

struct Slice
{
        ssize_t from = 0;
        ssize_t to = -1;

        struct Range
        {
                size_t from;
                size_t to;

                size_t size() const { return to - from; }
                bool empty() const { return from >= to; }

                operator bool() const { return !this->empty(); }
        };

        static Slice from_size(size_t offset, size_t size)
        {
            return Slice {static_cast<ssize_t>(offset), static_cast<ssize_t>(offset + size - 1)};
        }

        Range resolve(size_t size) const
        {
            const ssize_t sz = static_cast<ssize_t>(size);
            ssize_t f = from < 0 ? from + sz : from;
            ssize_t t = to < 0 ? to + sz : to;
            f = std::clamp(f, ssize_t(0), sz);
            t = std::clamp(t, ssize_t(-1), sz - 1);
            if (f > t)
                return {0, 0};
            return {static_cast<size_t>(f), static_cast<size_t>(t + 1)};
        }
};

} // namespace sihd::util

#endif
