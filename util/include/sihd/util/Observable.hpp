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
        Observable() {};
        virtual ~Observable() {};

        bool add_observer(IHandler<T *> *obs)
        {
            if (this->is_observer(obs))
                return false;
            std::lock_guard<std::mutex> l(_mutex);
            _observers.push_back(obs);
            return true;
        }

        bool remove_observer(IHandler<T *> *obs)
        {
            if (this->is_observer(obs) == false)
                return false;
            std::lock_guard<std::mutex> l(_mutex_remove);
            _observers_to_remove.push_back(obs);
            return true;
        }

        bool is_observer(IHandler<T *> *obs) const
        {
            return std::find(_observers.cbegin(), _observers.cend(), obs) != _observers.cend();
        }

        template <typename OBS_TYPE>
        OBS_TYPE *get_first_observer()
        {
            std::lock_guard<std::mutex> l(_mutex);
            for (IHandler<T *> *observer : _observers)
            {
                OBS_TYPE *casted = dynamic_cast<OBS_TYPE *>(observer);
                if (casted != nullptr)
                    return casted;
            }
            return nullptr;
        }

    protected:
        virtual void notify_observers(T *sender)
        {
            {
                std::lock_guard<std::mutex> rm_lock(_mutex_remove);
                for (const auto & obs_to_remove : _observers_to_remove)
                {
                    _observers.erase(std::find(_observers.begin(), _observers.end(), obs_to_remove));
                }
                _observers_to_remove.clear();
            }
            std::lock_guard<std::mutex> lock(_mutex);
            for (const auto & obs : _observers)
            {
                obs->handle(sender);
            }
        }

    private:
        std::mutex _mutex;
        std::mutex _mutex_remove;
        std::vector<IHandler<T *> *> _observers;
        std::list<IHandler<T *> *> _observers_to_remove;
};

} // namespace sihd::util

#endif