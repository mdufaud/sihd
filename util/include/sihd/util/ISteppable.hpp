#ifndef __SIHD_UTIL_ISTEPPABLE_HPP__
#define __SIHD_UTIL_ISTEPPABLE_HPP__

namespace sihd::util
{

class ISteppable
{
    public:
        virtual ~ISteppable() = default;
        ;
        virtual bool step() = 0;
};

} // namespace sihd::util

#endif