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

        virtual T *clone() const = 0;
};

}

#endif