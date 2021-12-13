#ifndef __SIHD_UTIL_IWRITER_HPP__
# define __SIHD_UTIL_IWRITER_HPP__

namespace sihd::util
{

class IWriter
{
    public:
        virtual ~IWriter() {};
		virtual ssize_t write(const char *data, size_t size) = 0;
};

class IWriterTimestamp: public IWriter
{
    public:
        virtual ~IWriterTimestamp() {};
		virtual ssize_t write(const char *data, size_t size, time_t nano_timestamp) = 0;
};

}

#endif