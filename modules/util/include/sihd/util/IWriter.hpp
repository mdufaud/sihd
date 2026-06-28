#ifndef __SIHD_UTIL_IWRITER_HPP__
#define __SIHD_UTIL_IWRITER_HPP__

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class IWriter
{
    public:
        virtual ~IWriter() = default;
        ;
        virtual ssize_t write(ArrCharView view) = 0;
};

class IWriterTimestamp: public IWriter
{
    public:
        using IWriter::write;

        virtual ~IWriterTimestamp() = default;
        ;
        virtual ssize_t write(ArrCharView view, Timestamp timestamp) = 0;
};

} // namespace sihd::util

#endif