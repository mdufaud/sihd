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
            _observers.erase(std::find(_observers.begin(), _observers.end(), obs));
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
            auto it = _observers.begin();
            while (it != _observers.end())
            {
                _current_observer = (*it);
                _current_observer->handle(sender);
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
        }

    private:
        mutable std::recursive_mutex _mutex;
        bool _remove_current_observer {false};
        IHandler<T *> *_current_observer {nullptr};
        std::list<IHandler<T *> *> _observers;
};

} // namespace sihd::util

#endif