#ifndef __SIHD_UTIL_SLICE_HPP__
#define __SIHD_UTIL_SLICE_HPP__

#include <sys/types.h>

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

                size_t size() const;
                bool empty() const;

                operator bool() const;
        };

        static Slice from_size(size_t offset, size_t size);
        Range resolve(size_t size) const;
};

} // namespace sihd::util

#endif
