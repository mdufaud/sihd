#ifndef __SIHD_UTIL_OBSERVABLE_HPP__
#define __SIHD_UTIL_OBSERVABLE_HPP__

#include <algorithm>
#include <functional>
#include <list>
#include <mutex>
#include <vector>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/IObservable.hpp>
#include <sihd/util/IObserverWatcher.hpp>

namespace sihd::util
{

template <typename T>
class Observable: public IObservable<T>
{
    public:
        Observable() = default;
        virtual ~Observable() = default;

        bool add_observer(IHandler<T *> *obs, bool add_to_front = false)
        {
            std::lock_guard l(_mutex);

            if (this->is_observer(obs))
                return false;

            if (add_to_front)
                _observers.emplace(_observers.begin(), obs);
            else
                _observers.emplace(_observers.end(), obs);

            return true;
        }

        void remove_observer(IHandler<T *> *obs)
        {
            std::lock_guard l(_mutex);
            if (_current_observer == obs)
            {
                _remove_current_observer = true;
                return;
            }
            auto it = std::find(_observers.begin(), _observers.end(), obs);
            if (it != _observers.end())
                _observers.erase(it);
        }

        bool is_observer(IHandler<T *> *obs) const
        {
            std::lock_guard l(_mutex);
            return std::find(_observers.cbegin(), _observers.cend(), obs) != _observers.cend();
        }

        size_t observers_count() const
        {
            std::lock_guard l(_mutex);
            return _observers.size();
        }

        // iterates a snapshot: fun may add or remove observers
        void for_each_observer(const std::function<void(IHandler<T *> *)> & fun)
        {
            std::vector<IHandler<T *> *> observers;
            {
                std::lock_guard l(_mutex);
                observers.assign(_observers.begin(), _observers.end());
            }
            for (IHandler<T *> *obs : observers)
                fun(obs);
        }

        // notified around every observer callback; read again at every hook
        // call, and must be detached before being destroyed
        void set_watcher(IObserverWatcher<T> *watcher)
        {
            std::lock_guard l(_mutex);
            _watcher = watcher;
        }

        IObserverWatcher<T> *watcher() const
        {
            std::lock_guard l(_mutex);
            return _watcher;
        }

    protected:
        virtual void notify_observers(T *sender)
        {
            std::lock_guard lock(_mutex);
            if (_watcher != nullptr) [[unlikely]]
                _watcher->before_notification(sender);
            auto it = _observers.begin();
            while (it != _observers.end())
            {
                IHandler<T *> *obs = *it;
                _current_observer = obs;
                if (_watcher == nullptr) [[likely]]
                    obs->handle(sender);
                else
                {
                    _watcher->before_observer(obs, sender);
                    obs->handle(sender);
                    // the observer may have detached the watcher
                    if (_watcher != nullptr)
                        _watcher->after_observer(obs, sender);
                }
                if (_remove_current_observer)
                {
                    _remove_current_observer = false;
                    it = _observers.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            _current_observer = nullptr;
            if (_watcher != nullptr) [[unlikely]]
                _watcher->after_notification(sender);
        }

    private:
        mutable std::recursive_mutex _mutex;
        bool _remove_current_observer {false};
        IHandler<T *> *_current_observer {nullptr};
        IObserverWatcher<T> *_watcher {nullptr};
        std::list<IHandler<T *> *> _observers;
};

} // namespace sihd::util

#endif
