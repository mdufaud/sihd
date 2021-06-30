#ifndef __SIHD_UTIL_ICLONABLE_HPP__
# define __SIHD_UTIL_ICLONABLE_HPP__

# include <memory>

namespace sihd::util
{

template <typename T>
class ICloneable
{
    public:
        virtual ~ICloneable() {};

        virtual std::unique_ptr<T>    clone() = 0;
};

}

#endif 