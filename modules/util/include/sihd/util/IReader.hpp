#ifndef __SIHD_UTIL_IREADER_HPP__
#define __SIHD_UTIL_IREADER_HPP__

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class IReader
{
    public:
        virtual ~IReader() = default;

        // false means end of read, an error means the read failed
        virtual sihd::util::Result<bool> read_next() = 0;
        virtual bool get_read_data(ArrCharView & view) const = 0;
};

class IReaderTimestamp: public IReader
{
    public:
        virtual ~IReaderTimestamp() = default;

        virtual sihd::util::Result<Timestamp> get_read_timestamp() const = 0;
};

} // namespace sihd::util

#endif
