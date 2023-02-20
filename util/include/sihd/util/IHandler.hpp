#ifndef __SIHD_UTIL_IHANDLER_HPP__
#define __SIHD_UTIL_IHANDLER_HPP__

namespace sihd::util
{

template <typename... T>
class IHandler
{
    public:
        virtual ~IHandler() {};
        virtual void handle(T... args) = 0;
};

} // namespace sihd::util

#endif