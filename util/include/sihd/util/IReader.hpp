#ifndef __SIHD_UTIL_IREADER_HPP__
#define __SIHD_UTIL_IREADER_HPP__

#include <sihd/util/ArrayView.hpp>

namespace sihd::util
{

class IReader
{
    public:
        virtual ~IReader() = default;
        ;
        virtual bool read_next() = 0;
        virtual bool get_read_data(ArrCharView & view) const = 0;
};

class IReaderTimestamp: public IReader
{
    public:
        virtual ~IReaderTimestamp() = default;
        ;
        virtual bool get_read_timestamp(time_t *nano_timestamp) const = 0;
};

} // namespace sihd::util

#endif