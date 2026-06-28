#ifndef __SIHD_UTIL_IRUNNABLE_HPP__
#define __SIHD_UTIL_IRUNNABLE_HPP__

namespace sihd::util
{

class IRunnable
{
    public:
        virtual ~IRunnable() = default;
        ;
        virtual bool run() = 0;
};

} // namespace sihd::util

#endif