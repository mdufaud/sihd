#ifndef __SIHD_UTIL_IREADER_HPP__
# define __SIHD_UTIL_IREADER_HPP__

namespace sihd::util
{

template <typename T>
class IReader
{
    public:
        virtual ~IReader() {};

        virtual T read() = 0;
};

}

#endif 