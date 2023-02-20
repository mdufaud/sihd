#ifndef __SIHD_UTIL_IOBSERVABLE_HPP__
#define __SIHD_UTIL_IOBSERVABLE_HPP__

namespace sihd::util
{

template <typename T>
class IObservable
{
    public:
        virtual ~IObservable() {};

    protected:
        virtual void notify_observers(T *sender) = 0;
};

} // namespace sihd::util

#endif