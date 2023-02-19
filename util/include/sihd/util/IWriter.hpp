#ifndef __SIHD_UTIL_IWRITER_HPP__
# define __SIHD_UTIL_IWRITER_HPP__

# include <ArrayView.hpp>

namespace sihd::util
{

class IWriter
{
    public:
        virtual ~IWriter() {};
		virtual ssize_t write(ArrCharView view) = 0;
};

class IWriterTimestamp: public IWriter
{
    using IWriter::write;

    public:
        virtual ~IWriterTimestamp() {};
		virtual ssize_t write(ArrCharView view, time_t nano_timestamp) = 0;
};

}

#endif