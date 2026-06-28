#ifndef __SIHD_UTIL_ICLONABLE_HPP__
#define __SIHD_UTIL_ICLONABLE_HPP__

namespace sihd::util
{

template <typename T>
class ICloneable
{
    public:
        virtual ~ICloneable() = default;
        ;

        virtual T *clone() const = 0;
};

} // namespace sihd::util

#endif