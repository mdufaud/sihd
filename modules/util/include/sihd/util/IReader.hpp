#ifndef __SIHD_UTIL_IREADER_HPP__
#define __SIHD_UTIL_IREADER_HPP__

#include <expected>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class IReader
{
    public:
        virtual ~IReader() = default;

        // false means end of read, an error means the read failed
        virtual std::expected<bool, Error> read_next() = 0;
        virtual bool get_read_data(ArrCharView & view) const = 0;
};

class IReaderTimestamp: public IReader
{
    public:
        virtual ~IReaderTimestamp() = default;

        virtual std::expected<Timestamp, Error> get_read_timestamp() const = 0;
};

} // namespace sihd::util

#endif
