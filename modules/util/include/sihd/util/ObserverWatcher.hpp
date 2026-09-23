#ifndef __SIHD_UTIL_OBSERVERWATCHER_HPP__
#define __SIHD_UTIL_OBSERVERWATCHER_HPP__

#include <functional>
#include <utility>

#include <sihd/util/IObserverWatcher.hpp>

namespace sihd::util
{

// a lambda-driven IObserverWatcher for quick debug hooks: unset hooks are no-ops
template <typename T>
class ObserverWatcher: public IObserverWatcher<T>
{
    public:
        using NotificationFun = std::function<void(T *)>;
        using ObserverFun = std::function<void(IHandler<T *> *, T *)>;

        ObserverWatcher() = default;

        // hooks in interface order, pass nullptr for the unwanted ones
        ObserverWatcher(NotificationFun before_notification,
                        NotificationFun after_notification,
                        ObserverFun before_observer,
                        ObserverFun after_observer):
            _before_notification(std::move(before_notification)),
            _after_notification(std::move(after_notification)),
            _before_observer(std::move(before_observer)),
            _after_observer(std::move(after_observer))
        {
        }

        void before_notification(T *sender) override
        {
            if (_before_notification)
                _before_notification(sender);
        }

        void after_notification(T *sender) override
        {
            if (_after_notification)
                _after_notification(sender);
        }

        void before_observer(IHandler<T *> *obs, T *sender) override
        {
            if (_before_observer)
                _before_observer(obs, sender);
        }

        void after_observer(IHandler<T *> *obs, T *sender) override
        {
            if (_after_observer)
                _after_observer(obs, sender);
        }

        void set_before_notification(NotificationFun fun) { _before_notification = std::move(fun); }
        void set_after_notification(NotificationFun fun) { _after_notification = std::move(fun); }
        void set_before_observer(ObserverFun fun) { _before_observer = std::move(fun); }
        void set_after_observer(ObserverFun fun) { _after_observer = std::move(fun); }

    private:
        NotificationFun _before_notification;
        NotificationFun _after_notification;
        ObserverFun _before_observer;
        ObserverFun _after_observer;
};

} // namespace sihd::util

#endif
