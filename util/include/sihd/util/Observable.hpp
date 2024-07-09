#ifndef __SIHD_UTIL_OBSERVABLE_HPP__
#define __SIHD_UTIL_OBSERVABLE_HPP__

#include <algorithm>
#include <list>
#include <mutex>
#include <vector>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/IObservable.hpp>

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

            const bool is_already_observer
                = std::find(_observers.cbegin(), _observers.cend(), obs) != _observers.cend();
            if (is_already_observer)
                return true;

            if (add_to_front)
                _observers.emplace(_observers.begin(), obs);
            else
                _observers.emplace_back(obs);
            return true;
        }

        void remove_observer(IHandler<T *> *obs)
        {
            std::lock_guard l(_mutex);
            _observers_to_remove.emplace_back(obs);
        }

        bool is_observer(IHandler<T *> *obs) const
        {
            std::lock_guard l(_mutex);
            return std::find(_observers.cbegin(), _observers.cend(), obs) != _observers.cend();
        }

    protected:
        virtual void notify_observers(T *sender)
        {
            std::lock_guard lock(_mutex);
            for (const auto & obs_to_remove : _observers_to_remove)
            {
                const auto it = std::find(_observers.begin(), _observers.end(), obs_to_remove);
                if (it != _observers.end())
                    _observers.erase(it);
            }
            _observers_to_remove.clear();
            for (const auto & obs : _observers)
            {
                obs->handle(sender);
            }
        }

    private:
        mutable std::recursive_mutex _mutex;
        std::vector<IHandler<T *> *> _observers;
        std::list<IHandler<T *> *> _observers_to_remove;
};

} // namespace sihd::util

#endif