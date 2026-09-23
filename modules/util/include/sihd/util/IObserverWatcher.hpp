#ifndef __SIHD_UTIL_IOBSERVERWATCHER_HPP__
#define __SIHD_UTIL_IOBSERVERWATCHER_HPP__

#include <sihd/util/IHandler.hpp>

namespace sihd::util
{

// observes the notifications of an Observable: once per notification with the
// envelope hooks, and around every observer callback
template <typename T>
class IObserverWatcher
{
    public:
        virtual ~IObserverWatcher() = default;

        virtual void before_notification([[maybe_unused]] T *sender) {}
        virtual void after_notification([[maybe_unused]] T *sender) {}

        virtual void before_observer(IHandler<T *> *obs, T *sender) = 0;
        virtual void after_observer(IHandler<T *> *obs, T *sender) = 0;
};

} // namespace sihd::util

#endif
