#ifndef __SIHD_UTIL_IREADER_HPP__
# define __SIHD_UTIL_IREADER_HPP__

namespace sihd::util
{

class IReader
{
    public:
        virtual ~IReader() {};
		virtual bool read_next() = 0;
		virtual bool get_read_data(char **data, size_t *size) const = 0;
};

class IReaderTimestamp: public IReader
{
    public:
        virtual ~IReaderTimestamp() {};
		virtual bool read_timestamp(time_t *nano_timestamp) const = 0;
};

}

#endif